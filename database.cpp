#include "database.h"

//erstellt eine neue ODBC Datenbankverbindung
Database::Database(QString connectionName)
{
    m_db = QSqlDatabase::addDatabase("QODBC", connectionName);
}

//Verbindung zu Datenbank aufbauen
void Database::Connect()
{
    QString connectString = QStringLiteral(
        "DRIVER={MySQL ODBC 8.0 Unicode Driver};"
        "SERVERNODE=127.0.0.1:3306;"
        "UID=root;"
        "PWD=vpj;");
    m_db.setDatabaseName(connectString);

    //konnte die Verbindung aufgebaut werden?
    if(m_db.open())
    {
        qDebug() << "Agent" << m_db.connectionName() << "connected to database!";
    }
    else
    {
        qDebug() << m_db.lastError().text();
    }
}

//Verbindung schließen/trennen
void Database::Disconnect()
{
    if(m_db.open())
    {
        m_db.close();
        qDebug() << "Agent" << m_db.connectionName() << "disconnected from database!";
    }
}

void Database::updateRobotHistory(int robotId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO vpj.robot_history (robot_position_x, robot_position_y, battery_level, station_place_id, jobtype_id ,state_id, robot_id) SELECT robot_position_x, robot_position_y, battery_level, station_place_id, jobtype_id ,state_id, robot_id FROM vpj.robot WHERE robot_id = :robot_id; ");
    query.bindValue(":robot_id", robotId);
    query.exec();
}

void Database::updateStationPlaceHistory(int stationPlaceId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_place_id = :station_place_id");
    query.bindValue(":station_place_id", stationPlaceId);
    query.exec();
}

void Database::updateStationPlaceHistory(int stationId, int placeId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_id = :station_id AND place_id = :place_id");
    query.bindValue(":station_id", stationId);
    query.bindValue(":place_id", placeId);
    query.exec();
}

void Database::updateWorkpieceHistory(int workpieceId)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO vpj.workpiece_history (rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id) SELECT rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id; ");
    query.bindValue(":workpiece_id", workpieceId);
    query.exec();
}

