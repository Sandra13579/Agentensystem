#ifndef GLOBAL_H
#define GLOBAL_H

#include <QList>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>

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

inline void updateRobotHistory(QSqlDatabase db, int robotId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO vpj.robot_history (robot_position_x, robot_position_y, battery_level, station_place_id, jobtype_id ,state_id, robot_id) SELECT robot_position_x, robot_position_y, battery_level, station_place_id, jobtype_id ,state_id, robot_id FROM vpj.robot WHERE robot_id = :robot_id; ");
    query.bindValue(":robot_id", robotId);
    query.exec();
}

inline void updateWorkpieceHistory(QSqlDatabase db, int workpieceId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO vpj.workpiece_history (rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id) SELECT rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id; ");
    query.bindValue(":workpiece_id", workpieceId);
    query.exec();
}

inline void updateStationPlaceHistory(QSqlDatabase db, int stationPlaceId)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_place_id = :station_place_id");
    query.bindValue(":station_place_id", stationPlaceId);
    query.exec();
}

#endif // GLOBAL_H
