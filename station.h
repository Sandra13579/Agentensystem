#ifndef STATION_H
#define STATION_H

#include "database.h"

class Station
{
public:
    Station();
    ~Station();
    void updateStation();
private:
    Database *database;
    void stationrelease();
    void maintenanceChargingStation();
    void workpieceProcessing();
};

#endif // STATION_H
