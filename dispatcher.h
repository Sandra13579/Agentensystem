#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "database.h"

class Dispatcher
{
public:
    Dispatcher();
    ~Dispatcher();
    void updateJobtype();
private:
    Database *database;
    void maintenace();
    void charging();
    void breakjob();
    void publishbreak(int station_id, int station_place_id, int robot_id);
    void transport();
};

#endif // DISPATCHER_H
