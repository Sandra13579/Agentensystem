#include "interface.h"

Interface::Interface(QObject *parent)
    : QObject{parent}
{
    m_mqttClient = new QMqttClient(parent);
    connect(m_mqttClient, &QMqttClient::stateChanged, this, &Interface::updateConnectionState);
    m_udpSocket = new QUdpSocket(parent);
    m_robotPositionWriter = new QTimer(parent);
    connect(m_robotPositionWriter, &QTimer::timeout, this, &Interface::writeRobotPositionsIntoDatabase);
    m_database = new Database("Interface");
    m_database->connect();

    QSqlQuery query(m_database->db());
    query.prepare("SELECT robot_position_x, robot_position_y FROM vpj.robot");
    query.exec();
    while (query.next())
    {
        Position pos;
        pos.x = query.record().value(0).toDouble();
        pos.y = query.record().value(1).toDouble();
        m_oldRobotPositions.append(pos);
    }

    for (int i = 1; i < 9; ++i)
    {
        m_rfidStationStates.insert(i, false);
    }

    m_robotPositionWriter->start(15);

    //QTimer::singleShot(3000, this, &Interface::SendTest);
}

Interface::~Interface()
{
    disconnect(m_robotPositionWriter, &QTimer::timeout, this, &Interface::writeRobotPositionsIntoDatabase);
    m_robotPositionWriter->stop();
    delete m_robotPositionWriter;
    m_database->disconnect();
    delete m_database;
    m_udpSocket->close();
    delete m_udpSocket;
    m_mqttClient->disconnectFromHost();
    delete m_mqttClient;
}

void Interface::connectToBroker(QString ip, int port)
{
    m_mqttClient->setHostname(ip);
    m_mqttClient->setPort(port);
    m_mqttClient->setUsername("VPJ");
    m_mqttClient->setPassword("R462");
    m_mqttClient->setClientId("Agentensystem");
    m_mqttClient->connectToHost();
}

void Interface::disconnectFromBroker()
{
    m_mqttClient->disconnectFromHost();
}

void Interface::readUdpData()
{
    while(m_udpSocket->hasPendingDatagrams())
    {
        QByteArray data = m_udpSocket->receiveDatagram().data();
        m_robotPositions.positions.clear();

        //Robot positions
        for (int i = 0; i < udpRobotCount * udpRobotSize; i += udpRobotSize)
        {
            Position pos = getRobotPosition(data, i);
            m_robotPositions.positions.append(pos);
            //qDebug() << "Robot" << (i / 32 + 1) << " --> x: " << pos.x << "y: " << pos.y << "phi: " << pos.phi << "e: " << pos.e;
        }
        //Timestamp
        m_robotPositions.timestamp = getTimestamp(data, udpRobotCount * udpRobotSize).toString("HH:mm:ss");
        //qDebug() << "timestamp:" << _robotPositions.timestamp;
        m_positionDataAvailable = true;
    }
}

void Interface::startUdpListening(int port)
{
    //Bind port of the UDP camera host
    m_udpSocket->bind(QHostAddress::Any, port);

    //Connect the "ready" signal from the camera host (UDP) to the "ReadUdpData" method of this class
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &Interface::readUdpData);

    qDebug() << "Listening on UDP Port" << m_udpSocket->localPort();
}

void Interface::writeRobotPositionsIntoDatabase()
{
    if (!m_positionDataAvailable)
        return;
    for (int i = 0; i < 4; i++)
    {
        if (m_robotPositions.positions[i].x == 0 && m_robotPositions.positions[i].y == 0)
            continue;
        double e = qAbs(m_robotPositions.positions[i].e / 2);
        //If new pos - old pos < e
        if (qAbs(m_robotPositions.positions[i].x - m_oldRobotPositions[i].x) < e &&
            qAbs(m_robotPositions.positions[i].y - m_oldRobotPositions[i].y) < e)
        {
            continue;
        }
        QSqlQuery query(m_database->db());
        query.prepare("UPDATE vpj.robot SET robot_position_x = :x, robot_position_y = :y WHERE robot_id = :robot_id");
        query.bindValue(":robot_id", i + 1);
        query.bindValue(":x", QString::number(m_robotPositions.positions[i].x, 'f', 2));
        query.bindValue(":y", QString::number(m_robotPositions.positions[i].y, 'f', 2));
        query.exec();
        m_oldRobotPositions[i] = m_robotPositions.positions[i];
    }
    //m_oldRobotPositions = m_robotPositions.positions;
    m_positionDataAvailable = false;
}

void Interface::writeBatteryLevelIntoDatabase(int robotId, int batteryLevel)
{
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.robot SET battery_level = :battery_level WHERE robot_id = :id");
    query.bindValue(":id", robotId);
    query.bindValue(":battery_level", batteryLevel);
    query.exec();
}

void Interface::getSubscriptionPayload(const QMqttMessage msg)
{
    /* Ordungsgemäßes Schließen der Anwendung testweise!!!!!!! */
    /* Einfach über MQTT den Payload "exit" schicken --------- */
    /* ------------------------------------------------------- */
    if (msg.payload() == "exit")
    {
        qDebug() << "Exit requested!";
        emit close();
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
            if (batLevel > 0)
            {
                writeBatteryLevelIntoDatabase(robotId, batLevel);
            }
        }
        //Robot status
        else if (topicLevel[2] == "Status")
        {
            State state = static_cast<State>(obj["status"].toInt(4)); //Default: 4 (Error)
            Place place = { obj["station_id"].toInt(), obj["place_id"].toInt() };
            int robotId = topicLevel[1].toInt();
            //qDebug() << "Robot " << robotId << "state is " << state;
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

void Interface::subscribeToTopics()
{
    m_subscriptionRobotStatus = getSubscription(topicRobotState);
    m_subscriptionChargingStation = getSubscription(topicChargingStation);
    m_subscriptionRobotEnergy = getSubscription(topicRobotEnergy);
    m_subscriptionRfidStation = getSubscription(topicRfidSerialNumber);
}

void Interface::unsubscribeAllTopics()
{
    m_subscriptionRobotStatus->unsubscribe();
    m_subscriptionChargingStation->unsubscribe();
    m_subscriptionRobotEnergy->unsubscribe();
    m_subscriptionRfidStation->unsubscribe();
}

void Interface::updateSubscriberState(QMqttSubscription::SubscriptionState state)
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

void Interface::updateConnectionState(QMqttClient::ClientState state)
{
    switch (state)
    {
    case QMqttClient::Connected:
        qDebug() << "MQTT Client with ID" << m_mqttClient->clientId() << "connected to broker:" << m_mqttClient->hostname() << ":" << m_mqttClient->port();
        subscribeToTopics();
        QTimer::singleShot(1, this, [this]() { emit connected(); });   //Zum Einschalten der Agenten
        break;
    case QMqttClient::Disconnected:
        qDebug() << "MQTT Client with ID" << m_mqttClient->clientId() << "disconnected!";
        QTimer::singleShot(1, this, [this]() { emit disconnected(); });   //Zum Einschalten der Agenten
        unsubscribeAllTopics();
        QTimer::singleShot(2000, this, &Interface::reconnectToBroker);
        break;
    case QMqttClient::Connecting:
        qDebug() << "MQTT connection pending...";
        break;
    }
}

void Interface::reconnectToBroker()
{
    m_mqttClient->connectToHost();
}

void Interface::sendJob(Job job, int robotNo)
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
    publishMqttMessage(QString(topicJob).replace("<No>", QString::number(robotNo)), payload);
}

void Interface::sendCheck(int robotNo)
{
    QJsonObject object
        {
            {"read", 1}
        };

    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    publishMqttMessage(QString(topicChecked).replace("<No>", QString::number(robotNo)), payload);
}

void Interface::sendCharging (bool chargingState, int stationId, int robotId)
{
    QJsonObject object
        {
            {"charge", chargingState},
            {"robot", robotId}
        };
    QString payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    publishMqttMessage(QString(topicCharging).replace("<No>", QString::number(stationId)), payload);
}

void Interface::sendAllRfidReadersOff()
{
    QJsonObject payload
        {
            {"Read", 0}
        };
    PublishMqttMessage(topicRfidReaderOn, QJsonDocument(payload).toJson(QJsonDocument::Compact), 0, true);
}

void Interface::sendRfidReaderOn(int stationId)
{
    QJsonObject payload
        {
            {"Read", stationId}
        };
    PublishMqttMessage(topicRfidReaderOn, QJsonDocument(payload).toJson(QJsonDocument::Compact), 0, true);
    m_rfidStationStates[stationId] = true;
}

void Interface::SendRfidReaderOff(int stationId)
{
    int activeStationCount = 0;
    foreach (bool state, m_rfidStationStates)
    {
        if (state)
        {
            activeStationCount++;
        }
    }
    if (activeStationCount == 1)
    {
        sendAllRfidReadersOff();
    }
    m_rfidStationStates[stationId] = false;
}

// Publish an MQTT message with with the given payload and topic
void Interface::publishMqttMessage(QString topic, QString payload)
{
    PublishMqttMessage(topic, payload, 0, false);
}

void Interface::PublishMqttMessage(QString topic, QString payload, quint8 qos, bool retain)
{
    QByteArray message;
    message.append(payload.toStdString());
    m_mqttClient->publish(QMqttTopicName(topic), message, qos, retain);
}

QMqttSubscription *Interface::getSubscription(QString topic)
{
    auto tmp = m_mqttClient->subscribe(QMqttTopicFilter(topic));
    connect(tmp, &QMqttSubscription::messageReceived, this, &Interface::getSubscriptionPayload);
    connect(tmp, &QMqttSubscription::stateChanged, this, &Interface::updateSubscriberState);
    qDebug() << "Subscribe to topic" << tmp->topic().filter();
    return tmp;
}

double Interface::getDoubleFromByteArray(const QByteArray data, int index, int offset)
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

Position Interface::getRobotPosition(const QByteArray data, int index)
{
    Position pos;
    
    pos.x = getDoubleFromByteArray(data, index, 0);
    pos.y = getDoubleFromByteArray(data, index, 8);
    pos.phi = getDoubleFromByteArray(data, index, 16);
    pos.e = getDoubleFromByteArray(data, index, 24);

    return pos;
}

QTime Interface::getTimestamp(const QByteArray data, int index)
{
    int hour = (int)getDoubleFromByteArray(data, index, 0);
    int min = (int)getDoubleFromByteArray(data, index, 8);
    int sec = (int)getDoubleFromByteArray(data, index, 16);

    return QTime(hour, min, sec);
}
