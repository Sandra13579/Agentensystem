#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <QObject>
#include <QSqlQuery>
#include <QSqlIndex>
#include <QtMath>
#include <QElapsedTimer>

#include "global.h"
#include "database.h"

class Dispatcher : public QObject
{
    Q_OBJECT
public:
    explicit Dispatcher(QObject *parent = nullptr);
    //Dispatcher();
    ~Dispatcher();
public slots:
    void updateJobtype();
private:
    Database *database;
    QElapsedTimer measurementTimer;
    void maintenace();
    void charging();
    void breakAfterJob();
    void publishBreak(int station_id, int station_place_id, int robot_id);
    void transport();
signals:
    void sendJob(Job job, int robotNo);
};

#endif // DISPATCHER_H
