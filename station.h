#ifndef STATION_H
#define STATION_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlIndex>
#include <QDateTime>

#include "global.h"
#include "database.h"

class Station : public QObject
{
    Q_OBJECT
public:
    explicit Station(QObject *parent = nullptr);
    ~Station();
public slots:
    void updateStation();
private:
    Database *database;
    void stationRelease();
    void maintenanceChargingStation();
    void workpieceProcessing();
};

#endif // STATION_H
