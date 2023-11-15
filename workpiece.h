#ifndef WORKPIECE_H
#define WORKPIECE_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlIndex>
#include <QElapsedTimer>

#include "global.h"
#include "database.h"

class Workpiece : public QObject
{
    Q_OBJECT
public:
    explicit Workpiece(QObject *parent = nullptr);
    ~Workpiece();
public slots:
    void updateOrder();
private:
    Database *database;
    QElapsedTimer measurementTimer;
    void orderAllocation();
    void orderAdministration();
};

#endif // WORKPIECE_H
