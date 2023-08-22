#include "dispatcher.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlIndex>
#include <QTimer>

Dispatcher::Dispatcher()
{
    /*updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &Dispatcher::updateJobtype);
    updateTimer->start(1500);*/

    database = new Database();
    database->Connect();
}

Dispatcher::~Dispatcher()
{
    database->Disconnect();
}

void Dispatcher::updateJobtype()
{
    this->maintenace();
    this->charging();
    this->breakjob();
    this->transport();
}

void Dispatcher::maintenace()
{
    QSqlQuery query, query2;
    query.prepare("SELECT robot_id FROM vpj.robot WHERE state_id = 0 AND maintenance = 1 AND jobtype_id != 2");
    database->Exec(&query);
    while(query.next())
    {
        // publish Auftrag mit (jobtype =2, destination{station=10,place_id=1})!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        query2.prepare("UPDATE vpj.robot SET jobtype_id = 2 WHERE robot_id = :robot_id");
        query2.bindValue(":robot_id", query.record().value(0).toInt());
        database->Exec(&query2);
    }
}

void Dispatcher::charging()
{

}

void Dispatcher::breakjob()
{

}

void Dispatcher::transport()
{

}
