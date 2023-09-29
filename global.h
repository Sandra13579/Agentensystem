#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QDebug>

enum class JobType
{
    Transport = 0,
    Charging = 1,
    Maintenance = 2,
    Pause = 3
};

enum class State
{    
    Available = 0,
    Assigned = 1,
    Reserved = 2,
    Inactive = 3,
    Fault = 4,
    ReadyForCharging = 5,
    ReadyForReading = 6,
    Charging = 7,
    Connected = 8,
    Initial = 9
};

struct Position
{
    Position() { x = 0, y = 0, phi = 0, e = 0; }
    double x;
    double y;
    double phi;
    double e;
};

struct RobotPositions
{
    RobotPositions() {}
    QList<Position> positions;
    QString timestamp;
};

struct Place
{
    int stationId;
    int placeId;
};

struct Job
{
    Job(JobType jT)
    {
        jobType = jT;
    }
    JobType jobType;
    Place start;
    Place destination;
};

inline QDebug operator<<(QDebug debug, const State state) {
    QDebugStateSaver stateSaver(debug);
    switch (state) {
    case State::Available:
        debug.nospace() << "Available";
        break;
    case State::Assigned:
        debug.nospace() << "Assigned";
        break;
    case State::Reserved:
        debug.nospace() << "Reserved";
        break;
    case State::Inactive:
        debug.nospace() << "Inactive";
        break;
    case State::Fault:
        debug.nospace() << "Fault";
        break;
    case State::ReadyForCharging:
        debug.nospace() << "ReadyForCharging";
        break;
    case State::ReadyForReading:
        debug.nospace() << "ReadyForReading";
        break;
    case State::Charging:
        debug.nospace() << "Charging";
        break;
    case State::Connected:
        debug.nospace() << "Connected";
        break;
    case State::Initial:
        debug.nospace() << "Initial";
        break;
    default:
        break;
    }
    return debug;
}

#endif // GLOBAL_H
