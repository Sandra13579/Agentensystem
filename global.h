#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>

enum class JobType
{
    Transport = 0,
    Charging = 1,
    Maintainance = 2,
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

struct Job
{
    Job() {}
    JobType jobType;
    struct {
        int stationId;
        int placeId;
    } start;
    struct {
        int stationId;
        int placeId;
    } destination;
};

#endif // GLOBAL_H
