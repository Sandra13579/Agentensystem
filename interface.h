#ifndef INTERFACE_H
#define INTERFACE_H

#include <QObject>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTimer>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>

#include <QCoreApplication>

#include "global.h"
#include "database.h"

class Interface : public QObject
{
    Q_OBJECT
public:
    explicit Interface(QObject *parent = nullptr);
    ~Interface();

    void ConnectToBroker(QString ip, int port = 1883);
    void StartUdpListening(int port);
    void DisconnectFromBroker();
    void UnsubscribeAllTopics();

    //Constants
    const int udpRobotSize = 32;
    const int udpRobotCount = 8;

    //MQTT subscriber topics
    const QString topicRobotState = "Robot/+/Status"; //GW2 -> Status robot x
    const QString topicChargingStation = "Charging/+/Connected"; //GW4 -> Status charging station x
    const QString topicRobotEnergy = "Robot/+/Energy"; //GW4 -> Battery level robot x

    //MQTT publisher topics
    const QString topicJob = "Robot/<No>/Move"; //job -> GW2
    const QString topicChecked = "Robot/<No>/Checked"; // rfid read -> GW2
    const QString topicCharging = "Charging/<No>/Load"; //begin/abort charging -> GW4

private:
    //MQTT
    QMqttClient *_mqttClient;
    QMqttSubscription *_subscriptionRobotStatus;
    QMqttSubscription *_subscriptionRobotEnergy;
    QMqttSubscription *_subscriptionChargingStation;

    //MQTT methods
    void PublishMqttMessage(QString topicStr, QString payloadStr);
    QMqttSubscription * GetSubscription(QString topic);

    //UDP
    QUdpSocket *_udpSocket;
    RobotPositions _robotPositions;
    bool _positionDataAvailable = false;

    //UDP payload parse methods
    double GetDoubleFromByteArray(const QByteArray data, int index, int offset);
    Position GetRobotPosition(const QByteArray data, int index);
    QTime GetTimestamp(const QByteArray data, int index);

    //Database write timer
    Database *_database;
    QTimer *_robotPositionWriter;

    void SubscribeToTopics();

signals:
    void robotStateChanged(int robotId, State state);
    void chargingStateChanged(int stationId, State state);
    void connected();
    void disconnected();

public slots:
    //MQTT payload methods
    void SendJob(Job job, int robotNo);
    void SendCheck(int robotNo);
    void SendCharging(bool charging, int robotNo);

    void SendTest();    //Test method for mqtt publish!

private slots:
    void ReadUdpData();    //Slot that reads the UDP payload
    void GetSubscriptionPayload(const QMqttMessage msg);
    void WriteRobotPositionsInDatabase();
    void WriteBatteryLevelIntoDatabase(int robotId, int batteryLevel);
    void WriteRobotStatusIntoDatabase(int robotId, State state);

    //Handle changes in subscriber state (un-/subscibed/pending) and connection state (dis-/connected/pending)
    void UpdateSubscriberState(QMqttSubscription::SubscriptionState state);
    void UpdateConnectionState(QMqttClient::ClientState state);

    void ReconnectToBroker();
};

#endif // INTERFACE_H
