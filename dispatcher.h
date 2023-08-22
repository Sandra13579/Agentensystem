#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "database.h"
#include <QTimer>

class Dispatcher
{
public:
    Dispatcher();
    ~Dispatcher();
private:
    QTimer* updateTimer;
    Database *database;
    void updateJobtype();
    void maintenace();
    void charging();
    void breakjob();
    void transport();
};

#endif // DISPATCHER_H
