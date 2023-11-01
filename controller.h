#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QThread>

#include "interface.h"
#include "dispatcher.h"
#include "station.h"
#include "workpiece.h"
#include "robot.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller();
    //void startWork();

private:
    Interface *m_interface;

    QThread *m_dispatcherAgentThread;
    QThread *m_robotAgentThread;
    QThread *m_stationAgentThread;
    QThread *m_workpieceAgentThread;

public slots:
    void setInactive();
signals:
    void operate();
};

#endif // CONTROLLER_H
