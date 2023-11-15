#ifndef ROBOT_H
#define ROBOT_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlIndex>
#include <QDateTime>
#include <QtMath>

#include "global.h"
#include "database.h"

class Robot : public QObject
{
    Q_OBJECT
public:
    explicit Robot(QObject *parent = nullptr);
    ~Robot();

public slots:
    void updateRobot();

    void updateRobotStatus(int robotId, State state, Place place);
    void updateChargingStationStatus(int stationId, State state);
    void continueReading(int stationId, int serialNumber); //Slot -> nach der RFID-Leseanfrage vom Agenten wird diese Funktion aufgerufen,
        //wenn die Seriennummer vom RFID Chip eingelesen und übermittelt wurde ->asynchron ohne "while (wait)"!!!

signals:
    //Call to turn on the RFID reader at the station
    void rfidOn(int stationId);
    //Call to turn off the RFID reader at the station
    void rfidOff(int stationId);
    //Call to send check
    void check(int robotNo);
    //Call to start/stop charging
    void charge(bool chargingState, int placeId, int robotId);

private:
    Database *m_database;

    //Save incoming state changes from MQTT
    QList<State> m_robotStates;
    QList<Place> m_robotPlaces;
    QList<State> m_chargingStationStates;

    void reading(int robotId);
    void checking(int robotId);
    void transportFinished(int robotId);
    void ladevorgang(int robotId);

    void transport();
    void charging();
    void maintenance();
};

#endif // ROBOT_H
