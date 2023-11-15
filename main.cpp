#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include "interface.h"
#include "dispatcher.h"
#include "station.h"
#include "workpiece.h"
#include "robot.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QTimer *cycleTimer = new QTimer(&a);

    //Interface initialization
    Interface *interface = new Interface(&a);
    interface->StartUdpListening(25000);
    interface->ConnectToBroker("192.168.0.11", 1883);
    //QObject::connect(interface, &Interface::disconnected, &a, QCoreApplication::quit);

    //Dispatcher agent initialization
    Dispatcher *dispatcher = new Dispatcher(&a);
    QObject::connect(cycleTimer, &QTimer::timeout, dispatcher, &Dispatcher::updateJobtype);
    QObject::connect(dispatcher, &Dispatcher::sendJob, interface, &Interface::SendJob);  //Auftrag senden -> GW2

    //Station agent initialization
    Station *station = new Station(&a);
    QObject::connect(cycleTimer, &QTimer::timeout, station, &Station::updateStation);

    //Workpiece agent initialization
    Workpiece *workpiece = new Workpiece(&a);
    QObject::connect(cycleTimer, &QTimer::timeout, workpiece, &Workpiece::updateOrder);

    //Robot agent initialization
    Robot *robot = new Robot(&a);
    QObject::connect(cycleTimer, &QTimer::timeout, robot, &Robot::updateRobot);
    QObject::connect(interface, &Interface::robotStateChanged, robot, &Robot::updateRobotStatus);   //Roboterstatus ändert sich
    QObject::connect(interface, &Interface::chargingStationStateChanged, robot, &Robot::updateChargingStationStatus);   //Ladestation

    QObject::connect(interface, &Interface::serialNumberRead, robot, &Robot::continueReading);

    //MQTT actions
    QObject::connect(robot, &Robot::rfidOn, interface, &Interface::SendRfidReaderOn);       //RFID Reader einschalten
    QObject::connect(robot, &Robot::rfidOff, interface, &Interface::SendRfidReaderOff);     //RFID Reader ausschalten
    QObject::connect(robot, &Robot::check, interface, &Interface::SendCheck);               //Einschecken bzw. Auschecken
    QObject::connect(robot, &Robot::charge, interface, &Interface::SendCharging);           //Ladevorgang starten

    //Start agent execution
    QObject::connect(interface, &Interface::connected, cycleTimer, [=]() { cycleTimer->start(1000); });
    QObject::connect(interface, &Interface::disconnected, cycleTimer, [=]() { cycleTimer->stop(); QCoreApplication::quit(); });

    return a.exec();
}


