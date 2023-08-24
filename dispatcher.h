#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "database.h"
#include <QTimer>

class Dispatcher
{
public:
    Dispatcher();
    ~Dispatcher();
    void updateJobtype();
private:
    QTimer* updateTimer;
    Database *database;
    //void updateJobtype();
    void maintenace();
    void charging();
    void breakjob();
    void publishbreak(int station_id, int station_place_id, int robot_id);
    void transport();
};

#endif // DISPATCHER_H
