#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QObject>
#include <QDebug>

#include "global.h"

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QString connectionName);
    void connect();  //Methode zur Herstellung der Datenbankverbindung
    void disconnect();  //Methode zur Trennung der Datenbankverbindung
    QSqlDatabase db() const { return m_db; } //Übergabe an query!

    void updateRobotState(int robotId, State state);
    void updateRobotState(int robotId, State state, Place place);
    void updateRobotHistory(int robotId);
    void updateStationPlaceState(int stationPlaceId, State state);
    void updateStationPlaceState(int stationId, int placeId, State state);
    void updateStationPlaceHistory(int stationPlaceId);
    void updateStationPlaceHistory(int stationId, int placeId);
    void updateWorkpieceHistory(int workpieceId);

private:
    QSqlDatabase m_db;    //repräsentiert die tatsächliche Datenbankverbindung

};

#endif // DATABASE_H
