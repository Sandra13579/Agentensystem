#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <QObject>
#include <QThread>

class Controller : public QObject
{
    Q_OBJECT
public:
    explicit Controller(QObject *parent = nullptr);
    ~Controller();
    //void startWork();

    enum ThreadAgent
    {
        All,
        DispatcherAgent,
        RobotAgent,
        StationAgent,
        WorkpieceAgent
    };

private:
    QThread *dispatcherAgentThread;
    QThread *robotAgentThread;
    QThread *stationAgentThread;
    QThread *workpieceAgentThread;

public slots:
    void setInactive(Controller::ThreadAgent agent);
signals:
    void operate(Controller::ThreadAgent agent);
};

#endif // CONTROLLER_H
