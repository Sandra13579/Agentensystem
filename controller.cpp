#include "controller.h"

Controller::Controller(QObject *parent)
    : QObject{parent}
{
    m_interface = new Interface(this);
    m_interface->StartUdpListening(25000);
    m_interface->ConnectToBroker("localhost", 1883);

    //Dispatcher Agent Thread
    m_dispatcherAgentThread = new QThread();
    Dispatcher *dispatcher = new Dispatcher();
    dispatcher->moveToThread(m_dispatcherAgentThread);
    connect(m_dispatcherAgentThread, &QThread::finished, dispatcher, &QObject::deleteLater);    //Thread zum Löschen vormerken
    connect(this, &Controller::operate, dispatcher, &Dispatcher::updateJobtype);                //Aufgabe ausführen -> upateJobtype()
    connect(dispatcher, &Dispatcher::sendJob, m_interface, &Interface::SendJob);                //Auftrag senden -> GW2
    //connect(dispatcher, &Dispatcher::resultReady, this, &Controller::handleResults);          //Thread ist fertig durchgelaufen -> Dauerschleife??
    m_dispatcherAgentThread->start();
    qDebug() << "Dispatcher thread is ready to operate";

    m_stationAgentThread = new QThread();
    Station *station = new Station();
    station->moveToThread(m_stationAgentThread);
    connect(m_stationAgentThread, &QThread::finished, station, &QObject::deleteLater);          //Thread zum Löschen vormerken
    connect(this, &Controller::operate, station, &Station::updateStation);                      //Aufgabe ausführen -> upateStation()
    //connect(station, &Station::resultReady, this, &Controller::handleResults);                //Thread ist fertig durchgelaufen -> Dauerschleife?
    m_stationAgentThread->start();
    qDebug() << "Station thread is ready to operate";

    m_workpieceAgentThread = new QThread();
    Workpiece *workpiece = new Workpiece();
    workpiece->moveToThread(m_workpieceAgentThread);
    connect(m_workpieceAgentThread, &QThread::finished, workpiece, &QObject::deleteLater);      //Thread zum Löschen vormerken
    connect(this, &Controller::operate, workpiece, &Workpiece::updateOrder);                    //Aufgabe ausführen -> upateOrder()
    //connect(workpiece, &Workpiece::resultReady, this, &Controller::handleResults);            //Thread ist fertig durchgelaufen -> Dauerschleife?
    m_workpieceAgentThread->start();
    qDebug() << "Workpiece thread is ready to operate";

    m_robotAgentThread = new QThread();
    Robot *robot = new Robot();
    robot->moveToThread(m_robotAgentThread);
    connect(m_robotAgentThread, &QThread::finished, robot, &QObject::deleteLater);              //Thread zum Löschen vormerken
    connect(this, &Controller::operate, robot, &Robot::updateRobot);                            //Aufgabe ausführen -> updateRobot()
    connect(m_interface, &Interface::robotStateChanged, robot, &Robot::updateRobotStatus);      //Roboterstatus ändert sich
    connect(m_interface, &Interface::chargingStationStateChanged, robot, &Robot::updateChargingStationStatus);   //Ladestation
    connect(m_interface, &Interface::serialNumberRead, robot, &Robot::continueReading);         //Seriennummer (RFID) gelesen
    connect(robot, &Robot::rfidOn, m_interface, &Interface::SendRfidReaderOn);                  //RFID Reader einschalten
    connect(robot, &Robot::rfidOff, m_interface, &Interface::SendAllRfidReadersOff);            //RFID Reader ausschalten
    connect(robot, &Robot::check, m_interface, &Interface::SendCheck);                          //Einschecken bzw. Auschecken
    connect(robot, &Robot::charge, m_interface, &Interface::SendCharging);                      //Ladevorgang starten
    //connect(robot, &Workpiece::resultReady, this, &Controller::handleResults);                //Thread ist fertig durchgelaufen -> Dauerschleife?
    m_robotAgentThread->start();
    qDebug() << "Robot thread is ready to operate";

    connect(m_interface, &Interface::connected, this, [=] () { emit operate(); });              //Alle Threads zur Ausführung bringen
}

Controller::~Controller()
{
    m_dispatcherAgentThread->quit();
    m_dispatcherAgentThread->wait();
    qDebug() << "Dispatcher thread finished";
    m_stationAgentThread->quit();
    m_stationAgentThread->wait();
    qDebug() << "Station thread finished";
    m_workpieceAgentThread->quit();
    m_workpieceAgentThread->wait();
    qDebug() << "Workpiece thread finished";
    m_robotAgentThread->quit();
    m_robotAgentThread->wait();
    qDebug() << "Robot thread finished";

    m_interface->deleteLater();
}

void Controller::setInactive()
{

}
