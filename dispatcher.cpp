#include "dispatcher.h"

Dispatcher::Dispatcher(QObject *parent)
    : QObject{parent}
{
    database = new Database("Dispatcher");
    database->Connect();
}

Dispatcher::~Dispatcher()
{
    database->Disconnect();
}

void Dispatcher::updateJobtype()
{
    this->breakAfterJob();
    this->maintenace();
    this->charging();
    this->transport();
}

void Dispatcher::breakAfterJob()
{
    //Pause - nach Transport
    QSqlQuery query(database->db());
    query.prepare("SELECT station.station_id, robot.station_place_id, robot.robot_id FROM vpj.robot JOIN vpj.station_place ON robot.station_place_id = station_place.station_place_id JOIN vpj.station ON station_place.station_id = station.station_id WHERE robot.state_id = 0 AND station.state_id = 1;");
    query.exec();
    while (query.next())
    {
        if (query.record().value(0).toInt() < 9)
        {
            publishBreak(query.record().value(0).toInt(),query.record().value(1).toInt(), query.record().value(2).toInt());
        }
    }
    //Pause - nach Laden
    query.prepare("SELECT robot.station_place_id, robot.robot_id FROM vpj.robot JOIN vpj.station_place ON robot.station_place_id = station_place.station_place_id WHERE robot.state_id = 0 AND station_place.state_id = 1 AND (station_place.station_place_id = 25 OR station_place.station_place_id = 26);");
    query.exec();
    while (query.next())
    {
        int stationPlaceId = query.record().value(0).toInt();
        int robotId = query.record().value(1).toInt();
        qDebug() << "Roboter" << robotId << "an Station" << stationPlaceId << "zur Pause schicken";
        publishBreak(9, stationPlaceId, robotId);
    }
}

void Dispatcher::publishBreak(int stationId, int stationPlaceId, int robotId)
{
    // publish Pausenauftrag (jobtype = 3, destination{station=10,place_id=1}) an robot_id
    Job job(JobType::Pause);
    job.start = {0, 0};
    job.destination = {10, 1};
    emit sendJob(job, robotId); // instruct to send a job (MQTT)
    if (stationId != 9) //Pause nach Transport
    {
        //qDebug() << "nach trans: publish stat_id " << station_id << ", place " << station_place_id << ", rob_id " << robot_id;
        //Station aktualisieren
        QSqlQuery query(database->db());
        query.prepare("UPDATE vpj.station SET clearing_time = NOW() WHERE station_id = :station_id;");
        query.bindValue(":station_id", stationId);
        query.exec();
    }
    else //Pause nach Laden
    {
        qDebug() << "Pause nach Laden";
        //qDebug() << "nach load: publish stat_id " << station_id << ", place " << station_place_id << ", rob_id " << robot_id;
        //Stationsplatz + Historie aktualisieren
        QSqlQuery query(database->db());
        query.prepare("UPDATE vpj.station_place SET clearing_time = NOW() WHERE station_place_id = :station_place_id;");
        query.bindValue(":station_place_id", stationPlaceId);
        query.exec();
        database->updateStationPlaceHistory(stationPlaceId);
    }
    //robotertabelle aktualisieren
    QSqlQuery query(database->db());
    query.prepare("UPDATE vpj.robot SET jobtype_id = 3, station_place_id = 27 WHERE robot_id = :robot_id;");
    query.bindValue(":robot_id", robotId);
    query.exec();
    database->updateRobotHistory(robotId);
}

void Dispatcher::maintenace()
{
    QSqlQuery query(database->db());
    query.prepare("SELECT robot_id FROM vpj.robot WHERE state_id = 0 AND maintenance = 1 AND jobtype_id = 3");
    query.exec();
    while(query.next())
    {
        int robotId = query.record().value(0).toInt();
        Job job(JobType::Maintenance);
        job.start = {0, 0};
        job.destination = {10, 1};
        // publish Wartungsauftrag (jobtype =2, destination{station=10,place_id=1}) an query.record().value(0).toInt()
        qDebug() << "Roboter" << robotId << "wird in Wartung geschickt";
        emit sendJob(job, robotId);

        //Robotertabelle + Historie aktualisieren
        QSqlQuery query2(database->db());
        query2.prepare("UPDATE vpj.robot SET jobtype_id = 2 WHERE robot_id = :robot_id");
        query2.bindValue(":robot_id", robotId);
        query2.exec();
        database->updateRobotHistory(robotId);
    }
}

void Dispatcher::charging()
{
    QSqlQuery query(database->db());
    //suche freie Ladestationen
    query.prepare("SELECT place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 0 AND maintenance = 0;");
    query.exec();
    while (query.next()) // solange eine freie Ladestation vorhanden
    {
        int placeId = query.record().value(0).toInt();
        //qDebug() << "Ladestation" << placeId << "ausgewählt, suche ladebereiten Roboter";

        //suche freie Roboter mit geringstem Battery Zustand, aber mindestens Batteriezustand < 30
        QSqlQuery query2(database->db());
        query2.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE battery_level < 30 AND maintenance = 0 AND state_id IN (0, 1, 2, 6) AND jobtype_id = 3 ORDER BY battery_level ASC");
        query2.exec();
        if (query2.next()) // wenn auch ein Roboter da ist, der geladen werden muss
        {
            int robotId = query2.record().value(0).toInt();
            State state = static_cast<State>(query2.record().value(1).toInt());
            qDebug() << "Roboter" << robotId << "mit Status" << state << "muss geladen werden";
            if (state == State::Available) // wenn freie Ladestation und freier Roboter
            {
                qDebug() << "Ladeauftrag erteilt für Roboter" << robotId;
                qDebug() << "Fahrt zur Ladestation" << placeId;
                // publish Ladeauftrag (jobtype =1, destination{station=9,place_id=query.record().value(0).toInt()}) an query2.record().value(0).toInt()
                Job job(JobType::Charging);
                job.start = {0, 0};
                job.destination = {9, placeId};
                emit sendJob(job, robotId); // instruct to send a job (MQTT)

                //Stationsplatztabelle + Historie aktualisieren (Platz reservieren)
                database->updateStationPlaceState(9, placeId, State::Reserved);
                database->updateStationPlaceHistory(9, placeId);
                //Robotertabelle + Historie aktualisieren
                QSqlQuery query3(database->db());
                query3.prepare("UPDATE vpj.robot SET jobtype_id = 1 WHERE robot_id = :robot_id;");
                query3.bindValue(":robot_id", robotId);
                query3.exec();
                database->updateRobotHistory(robotId);
            }
        }
    }
}


void Dispatcher::transport()
{
    int transportJobAllocated = 0;

    int workpiece_id, start_station_place_id, destinationStationId,  step_duration, robotId, step_id;
    double robot_workpiece_distance, distance;
    int alternative_destination_station_id = 0;
    int destination_station_place_id = -1;
    int destinationPlaceId = -1;
    int startStationId = -1;
    int startPlaceId = -1;

    //sind freie Roboter vorhanden?
    QSqlQuery query(database->db());
    query.prepare("SELECT robot_id FROM vpj.robot WHERE state_id = 0 AND jobtype_id = 3;");
    query.exec();
    if (query.next()) //wenn ein Roboter vorhanden ist
    {
        //nächster Bearbeitungsschritt:
        //Bestimmung des nächsten Schrittes für Werkstücke bei denen der Produktionsschritt abgeschlossen ist
        QSqlQuery query2(database->db());
        query2.prepare("SELECT workpiece_id, step_id, production_process_id, station_place_id FROM vpj.workpiece WHERE workpiece_state_id = 1 AND production_order_id IS not NULL ORDER BY TIMESTAMP ASC;");
        query2.exec();
        while (query2.next() && transportJobAllocated == 0)
        {
            workpiece_id = query2.record().value(0).toInt();
            start_station_place_id = query2.record().value(3).toInt();

            QSqlQuery query3(database->db());
            query3.prepare("SELECT sequence FROM vpj.production_step WHERE step_id = :step_id AND production_process_id = :production_process_id;");
            query3.bindValue(":step_id", query2.record().value(1).toInt());
            query3.bindValue(":production_process_id", query2.record().value(2).toInt());
            query3.exec();
            query3.next();

            //suche nächsten Schritt
            QSqlQuery query4(database->db());
            query4.prepare("SELECT step_id FROM vpj.production_step WHERE sequence = :sequence AND production_process_id = :production_process_id;");
            query4.bindValue(":sequence", query3.record().value(0).toInt() +1);
            query4.bindValue(":production_process_id", query2.record().value(2).toInt());
            query4.exec();
            query4.next();
            step_id = query4.record().value(0).toInt();

            if (query4.record().value(0).toInt() != 5) // wenn nächste Schritt_id eine Bearbeitungsmaschine
            {
                QSqlQuery query5(database->db());
                query5.prepare("SELECT station_id, step_duration FROM vpj.production_step WHERE sequence = :sequence AND production_process_id = :production_process_id;");
                query5.bindValue(":sequence", query3.record().value(0).toInt() + 1);
                query5.bindValue(":production_process_id", query2.record().value(2).toInt());
                query5.exec();
                query5.next();
                destinationStationId = query5.record().value(0).toInt();
                step_duration = query5.record().value(1).toInt();
            }
            else // wenn nächste Schritt_id eine Fertigteillager
            {
                destinationStationId = 7;
                alternative_destination_station_id = 8;
                step_duration = 0;
            }
            qDebug() << "workpiece_id" << workpiece_id << ", start_station_place_id" << start_station_place_id << ", destination_station_id" << destinationStationId << ", alternative_destination_station_id" << alternative_destination_station_id << ", step_duration" << step_duration;

            //nächste Werkstücke die bearbeitet werden können

            //gibt es einen nächsten freien Platz bei der Zielstation?
            query4.prepare("SELECT station_place_id, place_id FROM vpj.station_place WHERE station_id = :station_id AND state_id = 0 AND place_id != 1;");
            query4.bindValue(":station_id", destinationStationId);
            query4.exec();
            if (query4.next())
            {
                destination_station_place_id = query4.record().value(0).toInt();
                destinationPlaceId = query4.record().value(1).toInt();
            }
            else if (alternative_destination_station_id != 0)
            {
                query4.prepare("SELECT station_place_id, place_id FROM vpj.station_place WHERE station_id = :station_id AND state_id = 0 AND place_id != 1;");
                query4.bindValue(":station_id", alternative_destination_station_id);
                query4.exec();
                if (query4.next())
                {
                    destination_station_place_id = query4.record().value(0).toInt();
                    destinationPlaceId = query4.record().value(1).toInt();
                    destinationStationId = alternative_destination_station_id;
                }
            }

            qDebug() << "destination_station_place_id" << destination_station_place_id << ", destination_place_id" << destinationPlaceId << ", destination_station_id" << destinationStationId;
            if (destinationPlaceId != -1)
            {
                //steht kein Roboter an der aktuellen Station?
                QSqlQuery query6(database->db());
                query6.prepare("SELECT station.station_id, station_place.place_id FROM vpj.station INNER JOIN vpj.station_place ON station.station_id = station_place.station_id WHERE station_place.station_place_id = :station_place_id AND station.state_id = 0;");
                query6.bindValue(":station_place_id", start_station_place_id);
                query6.exec();
                if (query6.next())
                {
                    startStationId = query6.record().value(0).toInt();
                    startPlaceId = query6.record().value(1).toInt();
                    qDebug() << "start_station_id" << startStationId << "start_place_id" << startPlaceId;

                    //steht kein Roboter an der nächsten Station & die nächste Station muss nicht in Wartung?
                    QSqlQuery query7(database->db());
                    query7.prepare("SELECT station_id FROM vpj.station WHERE station_id = :station_id AND state_id = 0 AND maintenance = 0; ");
                    query7.bindValue(":station_id", destinationStationId);
                    query7.exec();
                    if (query7.next())
                    {
                        //Zuteilunsstrategie: kürzeste Strecke Roboter - Werkstück (Werkstück am längsten wartend)
                        qDebug() << "übergabe zuteilungsstrategie" << query7.record().value(0).toInt();

                        //Position des Werkstücks
                        query3.prepare("SELECT place_position_x, place_position_y FROM vpj.station_place INNER JOIN vpj.workpiece ON station_place.station_place_id = workpiece.station_place_id WHERE workpiece.workpiece_id = :workpiece_id;");
                        query3.bindValue(":workpiece_id", workpiece_id);
                        query3.exec();
                        query3.next();
                        //Position des Roboters
                        query4.prepare("SELECT robot_id, robot_position_x, robot_position_y FROM vpj.robot WHERE jobtype_id = 3;");
                        query4.bindValue(":workpiece_id", workpiece_id);
                        query4.exec();
                        if (query4.next())
                        {
                            robotId = query4.record().value(0).toInt();
                            robot_workpiece_distance = qSqrt(qPow((query3.record().value(0).toDouble() - query4.record().value(1).toDouble()), 2) + qPow((query3.record().value(1).toDouble() - query4.record().value(2).toDouble()), 2));
                            qDebug() << "robot_workpiece_distance" << robot_workpiece_distance;
                        }
                        while (query4.next())
                        {
                            distance = qSqrt(qPow((query3.record().value(0).toDouble() - query4.record().value(1).toDouble()), 2) + qPow((query3.record().value(1).toDouble() - query4.record().value(2).toDouble()), 2));
                            if (distance < robot_workpiece_distance)
                            {
                                robot_workpiece_distance = distance;
                                robotId = query4.record().value(0).toInt();
                                qDebug() << "new robot_workpiece_distance" << robot_workpiece_distance << "robot_id" << robotId;
                            }
                        }
                        qDebug() << "zugeteilter Roboter" << robotId << "step_duration" <<step_duration << "destination_station_place_id" << destination_station_place_id << "step_id" << step_id;

                        // publish Transportauftrag (jobtype = 0, start {station = start_station_id, place_id = start_place_id}, destination{station = destination_station_id, place_id = destination_place_id}) an robot_id
                        Job job(JobType::Transport);
                        job.start = {startStationId, startPlaceId};
                        job.destination = {destinationStationId, destinationPlaceId};
                        emit sendJob(job, robotId); // instruct to send a job (MQTT)

                        //Robotertabelle + History aktualisieren
                        query3.prepare("UPDATE vpj.robot SET jobtype_id = 0 WHERE robot_id = :robot_id;");
                        query3.bindValue(":robot_id", robotId);
                        query3.exec();
                        database->updateRobotHistory(robotId);

                        //Werkstücktabelle + History aktualisieren
                        query3.prepare("UPDATE vpj.workpiece SET current_step_duration = :current_step_duration, destination_station_place_id =  :destination_station_place_id, start_station_place_id =  :start_station_place_id, workpiece_state_id = 3, robot_id = :robot_id, step_id = :step_id WHERE workpiece_id = :workpiece_id;");
                        query3.bindValue(":current_step_duration", step_duration);
                        query3.bindValue(":destination_station_place_id", destination_station_place_id);
                        query3.bindValue(":start_station_place_id", start_station_place_id);
                        query3.bindValue(":robot_id", robotId);
                        query3.bindValue(":step_id", step_id);
                        query3.bindValue(":workpiece_id", workpiece_id);
                        query3.exec();
                        database->updateWorkpieceHistory(workpiece_id);

                        //Stationstabelle aktualisieren (Ziel und Start reservieren)
                        query3.prepare("UPDATE vpj.station SET state_id = 2 WHERE station_id = :start_station OR station_id = :destination_station;");
                        query3.bindValue(":start_station", startStationId);
                        query3.bindValue(":destination_station", destinationStationId);
                        query3.exec();
                        //Stationsplatztabelle + Historie aktualisieren (Zielplatz reservieren)
                        database->updateStationPlaceState(destination_station_place_id, State::Reserved);
                        database->updateStationPlaceHistory(destination_station_place_id);

                        transportJobAllocated = 1; //nach Zuteilung, Abbruch bedingung
                    }
                }
            }
        }
    }
}
