#include "interface.h"

Interface::Interface(QObject *parent)
    : QObject{parent}
{
    m_mqttClient = new QMqttClient(parent);
    connect(m_mqttClient, &QMqttClient::stateChanged, this, &Interface::UpdateConnectionState);
    m_udpSocket = new QUdpSocket(parent);
    m_robotPositionWriter = new QTimer(parent);
    connect(m_robotPositionWriter, &QTimer::timeout, this, &Interface::WriteRobotPositionsInDatabase);
    m_database = new Database("Interface");
    m_database->Connect();

    for (int i = 0; i < 8; ++i) {
        Position pos;
        m_oldRobotPositions.append(pos);
    }

    m_robotPositionWriter->start(15);

    //QTimer::singleShot(3000, this, &Interface::SendTest);
}

Interface::~Interface()
{
    disconnect(m_robotPositionWriter, &QTimer::timeout, this, &Interface::WriteRobotPositionsInDatabase);
    m_robotPositionWriter->stop();
    delete m_robotPositionWriter;
    m_database->Disconnect();
    delete m_database;
    m_udpSocket->close();
    delete m_udpSocket;
    m_mqttClient->disconnectFromHost();
    delete m_mqttClient;
}

void Interface::ConnectToBroker(QString ip, int port)
{
    m_mqttClient->setHostname(ip);
    m_mqttClient->setPort(port);
    m_mqttClient->setUsername("VPJ");
    m_mqttClient->setPassword("R462");
    m_mqttClient->setClientId("Agentensystem");
    m_mqttClient->connectToHost();
}

void Interface::DisconnectFromBroker()
{
    m_mqttClient->disconnectFromHost();
}

void Interface::ReadUdpData()
{
    while(m_udpSocket->hasPendingDatagrams())
    {
        QByteArray data = m_udpSocket->receiveDatagram().data();
        m_robotPositions.positions.clear();

        //Robot positions
        for (int i = 0; i < udpRobotCount * udpRobotSize; i += udpRobotSize)
        {
            Position pos = GetRobotPosition(data, i);
            m_robotPositions.positions.append(pos);
            //qDebug() << "Robot" << (i / 32 + 1) << " --> x: " << pos.x << "y: " << pos.y << "phi: " << pos.phi << "e: " << pos.e;
        }
        //Timestamp
        m_robotPositions.timestamp = GetTimestamp(data, udpRobotCount * udpRobotSize).toString("HH:mm:ss");
        //qDebug() << "timestamp:" << _robotPositions.timestamp;
        m_positionDataAvailable = true;
    }
}

void Interface::StartUdpListening(int port)
{
    //Bind port of the UDP camera host
    m_udpSocket->bind(QHostAddress::Any, port);

    //Connect the "ready" signal from the camera host (UDP) to the "ReadUdpData" method of this class
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Interface::ReadUdpData);

    qDebug() << "Listening on UDP Port" << m_udpSocket->localPort();
}

void Interface::WriteRobotPositionsInDatabase()
{
    if (!m_positionDataAvailable)
        return;
    for (int i = 0; i < 4; i++)
    {
        if (m_robotPositions.positions[i].x == 0 && m_robotPositions.positions[i].y == 0)
            continue;
        double e = m_robotPositions.positions[i].e;

        //If new pos - old pos < e
        if (m_robotPositions.positions[i].x - m_oldRobotPositions[i].x < e &&
            m_robotPositions.positions[i].y - m_oldRobotPositions[i].y < e)
        {
            break;
        }
        QSqlQuery query(m_database->db());
        query.prepare("UPDATE vpj.robot SET robot_position_x = :x, robot_position_y = :y WHERE robot_id = :id");
        query.bindValue(":id", i + 1);
        query.bindValue(":x", QString::number(m_robotPositions.positions[i].x, 'f', 2));
        query.bindValue(":y", QString::number(m_robotPositions.positions[i].y, 'f', 2));
        query.exec();
    }
    m_oldRobotPositions = m_robotPositions.positions;
    m_positionDataAvailable = false;
}

void Interface::WriteBatteryLevelIntoDatabase(int robotId, int batteryLevel)
{
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.robot SET battery_level = :battery_level WHERE robot_id = :id");
    query.bindValue(":id", robotId);
    query.bindValue(":battery_level", batteryLevel);
    query.exec();
}

//void Interface::WriteRobotStatusIntoDatabase(int robotId, State state, int stationId, int placeId)
//{
//    QSqlQuery query(m_database->db());
//    query.prepare("SELECT station_place_id FROM vpj.station_place WHERE station_id = :station_id AND place_id = :place_id");
//    query.bindValue(":station_id", stationId);
//    query.bindValue(":place_id", placeId);
//    query.exec();
//    if (query.next())
//    {
//        int stationPlaceId = query.record().value(0).toInt();
//        query.prepare("UPDATE vpj.robot SET state_id = :state_id, station_place_id = :station_place_id WHERE robot_id = :id");
//        query.bindValue(":id", robotId);
//        query.bindValue(":state_id", static_cast<int>(state));
//        query.bindValue(":station_place_id", stationPlaceId);
//        query.exec();
//        return;
//    }

//    //Falls Roboter nicht an einer Station steht wird der Status trotzdem aktualisiert (evtl. für Fehler etc.)
//    query.prepare("UPDATE vpj.robot SET state_id = :state_id WHERE robot_id = :id");
//    query.bindValue(":id", robotId);
//    query.bindValue(":state_id", static_cast<int>(state));
//    query.exec();
//}

//void Interface::WriteChargingStationStateIntoDatabase(int placeId, State state)
//{
//    QSqlQuery query(m_database->db());
//    query.prepare("UPDATE vpj.station_place SET state_id = :state_id WHERE station_id = 9 AND place_id = :place_id");
//    query.bindValue(":state_id", static_cast<int>(state));
//    query.bindValue(":place_id", placeId);
//    query.exec();
//}

void Interface::GetSubscriptionPayload(const QMqttMessage msg)
{
    /* Ordungsgemäßes Schließen der Anwendung testweise!!!!!!! */
    /* Einfach über MQTT den Payload "exit" schicken --------- */
    /* ------------------------------------------------------- */
    if (msg.payload() == "exit")
    {
        qDebug() << "Exit requested!";
        emit disconnected();
        return;
    }
    /* ------------------------------------------------------- */

    //qDebug() << "Payload:" << msg.payload().toStdString() <<", Topic:" << msg.topic().name().toStdString();
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
            //qDebug() << "Battery Level robot " << robotId << ":" << batLevel << "%";
            WriteBatteryLevelIntoDatabase(robotId, batLevel);
        }
        //Robot status
        else if (topicLevel[2] == "Status")
        {
            State state = static_cast<State>(obj["status"].toInt(4)); //Default: 4 (Error)
            Place place = { obj["station_id"].toInt(), obj["place_id"].toInt() };
            int robotId = topicLevel[1].toInt();
            qDebug() << "Robot " << robotId << "state is " << state;
            emit robotStateChanged(robotId, state, place);
        }
    }
    //Charging Station handling
    else if (topicLevel[0] == "Charging" && topicLevel[2] == "Connected")
    {
        State state = static_cast<State>(obj["status"].toInt(4)); //Default: 4 (Error)
        int placeId = topicLevel[1].toInt();
        //qDebug() << "Charging station" << placeId << "state is " << state;
        emit chargingStationStateChanged(placeId, state);
    }
    //Reading serial number from RFID tag
    else if (topicLevel[0] == "RFID" && topicLevel[3] == "Read")
    {
        int serialNumber = obj["serial_number"].toInt();
        int stationId = topicLevel[2].toInt();
        qDebug() << "Read serial number" << serialNumber << "from RFID Station" << stationId;
        emit serialNumberRead(stationId, serialNumber);
    }
}

void Interface::SubscribeToTopics()
{
    m_subscriptionRobotStatus = GetSubscription(topicRobotState);
    m_subscriptionChargingStation = GetSubscription(topicChargingStation);
    m_subscriptionRobotEnergy = GetSubscription(topicRobotEnergy);
    m_subscriptionRfidStation = GetSubscription(topicRfidSerialNumber);
}

void Interface::UnsubscribeAllTopics()
{
    m_subscriptionRobotStatus->unsubscribe();
    delete m_subscriptionRobotStatus;
    m_subscriptionChargingStation->unsubscribe();
    delete m_subscriptionChargingStation;
    m_subscriptionRobotEnergy->unsubscribe();
    delete m_subscriptionRobotEnergy;
    m_subscriptionRfidStation->unsubscribe();
    delete m_subscriptionRfidStation;
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
        qDebug() << "MQTT Client with ID" << m_mqttClient->clientId() << "connected to broker:" << m_mqttClient->hostname() << ":" << m_mqttClient->port();
        SubscribeToTopics();
        QTimer::singleShot(1, this, [this]() { emit connected(); });   //Zum Einschalten der Agenten
        break;
    case QMqttClient::Disconnected:
        qDebug() << "MQTT Client with ID" << m_mqttClient->clientId() << "disconnected!";
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
    m_mqttClient->connectToHost();
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

void Interface::SendCharging (bool chargingState, int stationId)
{
    QJsonObject object
        {
            {"charge", chargingState}
        };

    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    PublishMqttMessage(QString(topicCharging).replace("<No>", QString::number(stationId)), payload);
}

void Interface::SendAllRfidReadersOff()
{
    QJsonObject payload
        {
            {"Read", 0}
        };
    PublishMqttMessage(topicRfidReaderOn, QJsonDocument(payload).toJson(QJsonDocument::Compact), 0, true);
}

void Interface::SendRfidReaderOn(int stationId)
{
    QJsonObject payload
        {
            {"Read", stationId}
        };
    PublishMqttMessage(topicRfidReaderOn, QJsonDocument(payload).toJson(QJsonDocument::Compact), 0, true);
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
    PublishMqttMessage(topic, payload, 0, false);
}

void Interface::PublishMqttMessage(QString topic, QString payload, quint8 qos, bool retain)
{
    QByteArray message;
    message.append(payload.toStdString());
    m_mqttClient->publish(QMqttTopicName(topic), message, qos, retain);
}

QMqttSubscription *Interface::GetSubscription(QString topic)
{
    auto tmp = m_mqttClient->subscribe(QMqttTopicFilter(topic));
    connect(tmp, &QMqttSubscription::messageReceived, this, &Interface::GetSubscriptionPayload);
    connect(tmp, &QMqttSubscription::stateChanged, this, &Interface::UpdateSubscriberState);
    qDebug() << "Subscribe to topic" << tmp->topic().filter();
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
