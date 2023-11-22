#ifndef INTERFACE_H
#define INTERFACE_H

#include <QObject>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTimer>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtMath>
#include <QMap>

#include "global.h"
#include "database.h"

class Interface : public QObject
{
    Q_OBJECT
public:
    explicit Interface(QObject *parent = nullptr);
    ~Interface();

    void connectToBroker(QString ip, int port = 1883);
    void startUdpListening(int port);
    void disconnectFromBroker();
    void unsubscribeAllTopics();

    //Constants
    const int udpRobotSize = 32;
    const int udpRobotCount = 8;

    //MQTT subscriber topics
    const QString topicRobotState = "Robot/+/Status";            //GW2 -> Status robot x
    const QString topicChargingStation = "Charging/+/Connected"; //GW4 -> Status charging station x
    const QString topicRobotEnergy = "Robot/+/Energy";           //GW4 -> Battery level robot x
    const QString topicRfidSerialNumber = "RFID/Station/+/Read"; //Station -> Serial number of the RFID tag

    //MQTT publisher topics
    const QString topicJob = "Robot/<No>/Move";         //job -> GW2
    const QString topicChecked = "Robot/<No>/Checked";  // rfid read -> GW2
    const QString topicCharging = "Charging/<No>/Load"; //begin/abort charging -> GW4
    const QString topicRfidReaderOn = "RFID/Read_ON";   //switch rfid reader(s) on/off -> Rfid station

private:
    //MQTT
    QMqttClient *m_mqttClient;
    QMqttSubscription *m_subscriptionRobotStatus = nullptr;
    QMqttSubscription *m_subscriptionRobotEnergy = nullptr;
    QMqttSubscription *m_subscriptionChargingStation = nullptr;
    QMqttSubscription *m_subscriptionRfidStation = nullptr;

    //MQTT methods
    void publishMqttMessage(QString topic, QString payload);
    void PublishMqttMessage(QString topic, QString payload, quint8 qos, bool retain);
    void subscribeToTopics();
    QMqttSubscription *getSubscription(QString topic);
    QMap<int, bool> m_rfidStationStates;

    //UDP
    QUdpSocket *m_udpSocket;
    RobotPositions m_robotPositions;
    QList<Position> m_oldRobotPositions;
    bool m_positionDataAvailable = false;

    //UDP payload parse methods
    double getDoubleFromByteArray(const QByteArray data, int index, int offset);
    Position getRobotPosition(const QByteArray data, int index);
    QTime getTimestamp(const QByteArray data, int index);

    //Database write timer
    Database *m_database;
    QTimer *m_robotPositionWriter;

signals:
    void close(void);
    void connected(void);
    void disconnected(void);
    void robotStateChanged(int robotId, State state, Place place);
    void chargingStationStateChanged(int placeId, State state);
    void serialNumberRead(int stationId, int serialNumber);

public slots:
    //MQTT payload methods
    void sendJob(Job job, int robotNo);
    void sendCheck(int robotNo);
    void sendCharging(bool chargingState, int stationId, int robotId);
    void sendAllRfidReadersOff();
    void sendRfidReaderOn(int stationId);
    void SendRfidReaderOff(int stationId);

private slots:
    void readUdpData();    //Slot that reads the UDP payload
    void getSubscriptionPayload(const QMqttMessage msg);
    void writeRobotPositionsIntoDatabase();
    void writeBatteryLevelIntoDatabase(int robotId, int batteryLevel);

    //Handle changes in subscriber state (un-/subscibed/pending) and connection state (dis-/connected/pending)
    void updateSubscriberState(QMqttSubscription::SubscriptionState state);
    void updateConnectionState(QMqttClient::ClientState state);
    void reconnectToBroker();
};

#endif // INTERFACE_H
