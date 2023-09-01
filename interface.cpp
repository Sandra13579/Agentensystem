#include "interface.h"

Interface::Interface(QObject *parent)
    : QObject{parent}
{
    _mqttClient = new QMqttClient(parent);
    connect(_mqttClient, &QMqttClient::stateChanged, this, &Interface::UpdateConnectionState);
    _udpSocket = new QUdpSocket(parent);
    _robotPositionWriter = new QTimer(parent);
    connect(_robotPositionWriter, &QTimer::timeout, this, &Interface::WriteRobotPositionsInDatabase);
    _database = new Database("Interface");
    _database->Connect();

    _robotPositionWriter->start(1000);

    //QTimer::singleShot(3000, this, &Interface::SendTest);
}

Interface::~Interface()
{
    _robotPositionWriter->stop();
    _database->Disconnect();
    _udpSocket->close();
    _mqttClient->disconnectFromHost();
}

void Interface::ConnectToBroker(QString ip, int port)
{
    _mqttClient->setHostname(ip);
    _mqttClient->setPort(port);
    _mqttClient->setUsername("VPJ");
    _mqttClient->setPassword("R462");
    _mqttClient->connectToHost();
}

void Interface::DisconnectFromBroker()
{
    _mqttClient->disconnectFromHost();
}

void Interface::ReadUdpData()
{
    while(_udpSocket->hasPendingDatagrams())
    {
        QByteArray data = _udpSocket->receiveDatagram().data();
        _robotPositions.positions.clear();

        //Robot positions
        for (int i = 0; i < udpRobotCount * udpRobotSize; i += udpRobotSize)
        {
            Position pos = GetRobotPosition(data, i);
            _robotPositions.positions.append(pos);
            //qDebug() << "Robot" << (i / 32 + 1) << " --> x: " << pos.x << "y: " << pos.y << "phi: " << pos.phi << "e: " << pos.e;
        }
        //Timestamp
        _robotPositions.timestamp = GetTimestamp(data, udpRobotCount * udpRobotSize).toString("HH:mm:ss");
        //qDebug() << "timestamp:" << _robotPositions.timestamp;
        _positionDataAvailable = true;
    }
}

void Interface::StartUdpListening(int port)
{
    //Bind port of the UDP camera host
    _udpSocket->bind(QHostAddress::Any, port);

    //Connect the "ready" signal from the camera host (UDP) to the "ReadUdpData" method of this class
    connect(_udpSocket, &QUdpSocket::readyRead, this, &Interface::ReadUdpData);

    qDebug() << "Listening on UDP Port" << _udpSocket->localPort();
}

void Interface::WriteRobotPositionsInDatabase()
{
    if (!_positionDataAvailable)
        return;

    qDebug() << "Writing position data into the database.";
    for (int i = 0; i < 4; i++)
    {
        QSqlQuery query(_database->db());
        query.prepare("UPDATE vpj.robot SET robot_position_x = :x, robot_position_y = :y, TIMESTAMP = :timestamp WHERE robot_id = :id");
        query.bindValue(":id", i + 1);
        query.bindValue(":x", _robotPositions.positions[i].x);
        query.bindValue(":y", _robotPositions.positions[i].y);
        query.bindValue(":timestamp", QDate::currentDate().toString("yyyy-MM-dd") + " " + _robotPositions.timestamp);
        query.exec();
    }
    _positionDataAvailable = false;
}

void Interface::WriteBatteryLevelIntoDatabase(int robotId, int batteryLevel)
{
    QSqlQuery query(_database->db());
    query.prepare("UPDATE vpj.robot SET battery_level = :battery_level WHERE robot_id = :id");
    query.bindValue(":id", robotId);
    query.bindValue(":battery_level", batteryLevel);
    query.exec();
}

void Interface::WriteRobotStatusIntoDatabase(int robotId, State state)
{
    QSqlQuery query(_database->db());
    query.prepare("UPDATE vpj.robot SET state_id = :state_id WHERE robot_id = :id");
    query.bindValue(":id", robotId);
    query.bindValue(":state_id", static_cast<int>(state));
    query.exec();
}

void Interface::GetSubscriptionPayload(const QMqttMessage msg)
{
    /* Ordungsgemäßes Schließen der Anwendung testweise!!!!!!! */
    /* Einfach über MQTT den Payload "exit" schicken --------- */
    /* ------------------------------------------------------- */
    if (msg.payload() == "exit")
        QCoreApplication::exit();
    /* ------------------------------------------------------- */

    qDebug() << "Payload:" << msg.payload().toStdString() << ", Topic:" << msg.topic().name().toStdString();
    QJsonObject obj = QJsonDocument::fromJson(msg.payload()).object();
    QStringList topicLevel = msg.topic().levels();

    //Robot handling
    if (topicLevel[0] == "Robot")
    {
        //Battery Level of the robot
        if (topicLevel[2] == "Energy")
        {
            int batLevel = obj["battery_power"].toInt(-1);
            int robotId = topicLevel[1].toInt();
            WriteBatteryLevelIntoDatabase(robotId, batLevel);
            qDebug() << "Battery Level: " << batLevel << "%";
        }
        //Robot status changing
        else if (topicLevel[2] == "Status")
        {
            State state = static_cast<State>(obj["status"].toInt(4)); //Default: 4 (Error)
            int robotId = topicLevel[1].toInt();
            WriteRobotStatusIntoDatabase(robotId, state);
            emit robotStateChanged(robotId, state);
            qDebug() << "Robot state: " << obj["status"].toInt(4);
        }
    }
    //Charging Station handling
    else if (topicLevel[0] == "Charging" && topicLevel[2] == "Connected")
    {
        State state = static_cast<State>(obj["status"].toInt(4)); //Default: 4 (Error)
        int stationId = topicLevel[1].toInt();
        emit chargingStateChanged(stationId, state);
        qDebug() << "Charging station state: " << obj["status"].toInt(4);
    }
}

void Interface::SubscribeToTopics()
{
    _subscriptionRobotStatus = GetSubscription(topicRobotState);
    _subscriptionChargingStation = GetSubscription(topicChargingStation);
    _subscriptionRobotEnergy = GetSubscription(topicRobotEnergy);
}

void Interface::UnsubscribeAllTopics()
{
    _subscriptionRobotStatus->unsubscribe();
    _subscriptionChargingStation->unsubscribe();
    _subscriptionRobotEnergy->unsubscribe();
}

void Interface::UpdateSubscriberState(QMqttSubscription::SubscriptionState state)
{
    switch (state)
    {
    case QMqttSubscription::Subscribed:
        qDebug() << "Subscription succesfull";
        break;
    case QMqttSubscription::Unsubscribed:
        qDebug() << "Unsubscribed from topics.";
        break;
    case QMqttSubscription::SubscriptionPending:
    case QMqttSubscription::UnsubscriptionPending:
        qDebug() << "Subscription pending...";
        break;
    case QMqttSubscription::Error:
        qDebug() << "Error in Subscription.";
        break;
    }
}

void Interface::UpdateConnectionState(QMqttClient::ClientState state)
{
    switch (state)
    {
    case QMqttClient::Connected:
        qDebug() << "MQTT Client with ID" << _mqttClient->clientId() << "connected to broker:" << _mqttClient->hostname() << ":" << _mqttClient->port();
        emit connected();
        SubscribeToTopics();
        break;
    case QMqttClient::Disconnected:
        qDebug() << "MQTT Client with ID" << _mqttClient->clientId() << "disconnected!";
        emit disconnected();
        UnsubscribeAllTopics();
        QTimer::singleShot(2000, this, &Interface::ReconnectToBroker);
        break;
    case QMqttClient::Connecting:
        qDebug() << "MQTT connection pending...";
        break;
    }
}

void Interface::ReconnectToBroker()
{
    _mqttClient->connectToHost();
}

void Interface::SendJob(Job job, int robotNo)
{
    QJsonObject start
        {
            {"station_id", job.start.stationId},
            {"place_id", job.start.placeId}
        };
    QJsonObject destination
        {
            {"station_id", job.destination.stationId},
            {"place_id", job.destination.placeId}
        };
    QJsonObject object
        {
            {"jobtype", static_cast<int>(job.jobType)},
            {"start", start},
            {"destination", destination}
        };

    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    PublishMqttMessage(QString(topicJob).replace("<No>", QString::number(robotNo)), payload);
}

void Interface::SendCheck(int robotNo)
{
    QJsonObject object
        {
            {"read", 1}
        };

    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    PublishMqttMessage(QString(topicChecked).replace("<No>", QString::number(robotNo)), payload);
}

void Interface::SendCharging (bool charging, int robotNo)
{
    QJsonObject object
        {
            {"charge", charging}
        };

    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    PublishMqttMessage(QString(topicCharging).replace("<No>", QString::number(robotNo)), payload);
}

// SendTest: Only for testing!
void Interface::SendTest()
{
    SendCheck(4);
    Job job(JobType::Transport);
    job.start = {1,2};
    job.destination = {1,3};
    SendJob(job, 2);
    SendCharging(true, 3);
    SendCharging(false, 2);
}

// Publish an MQTT message with with the given payload and topic
void Interface::PublishMqttMessage(QString topic, QString payload)
{
    QByteArray message;
    message.append(payload.toStdString());
    _mqttClient->publish(QMqttTopicName(topic), message);
}

QMqttSubscription *Interface::GetSubscription(QString topic)
{
    qDebug() << "Subscribe to topic" << topic;
    auto tmp = _mqttClient->subscribe(QMqttTopicFilter(topic));
    connect(tmp, &QMqttSubscription::messageReceived, this, &Interface::GetSubscriptionPayload);
    connect(tmp, &QMqttSubscription::stateChanged, this, &Interface::UpdateSubscriberState);
    return tmp;
}

double Interface::GetDoubleFromByteArray(const QByteArray data, int index, int offset)
{
    QByteArray sub(8, 0);
    double value = 0;

    for (int i = 0; i < 8; i++) {
        sub[i]=data[index + i + offset];
    }
    QDataStream stream(sub);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream >> value;

    return value;
}

Position Interface::GetRobotPosition(const QByteArray data, int index)
{
    Position pos;

    pos.x = GetDoubleFromByteArray(data, index, 0);
    pos.y = GetDoubleFromByteArray(data, index, 8);
    pos.phi = GetDoubleFromByteArray(data, index, 16);
    pos.e = GetDoubleFromByteArray(data, index, 24);

    return pos;
}

QTime Interface::GetTimestamp(const QByteArray data, int index)
{
    int hour = (int)GetDoubleFromByteArray(data, index, 0);
    int min = (int)GetDoubleFromByteArray(data, index, 8);
    int sec = (int)GetDoubleFromByteArray(data, index, 16);

    return QTime(hour, min, sec);
}
