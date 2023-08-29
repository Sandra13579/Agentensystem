#ifndef WORKPIECE_H
#define WORKPIECE_H

#include "database.h"

class Workpiece
{
public:
    Workpiece();
    ~Workpiece();
    void updateOrder();
private:
    Database *database;
    void orderAllocation();
    void orderAdministration();
};

#endif // WORKPIECE_H
