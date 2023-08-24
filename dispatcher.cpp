#include "dispatcher.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlIndex>
#include <QTimer>

Dispatcher::Dispatcher()
{
    database = new Database();
    database->Connect();

    /*QTimer updateTimer;
    // connect(updateTimer, &QTimer::timeout, this, &Dispatcher::updateJobtype);
    updateTimer.start(1500);*/


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
        // publish Wartungsauftrag (jobtype =2, destination{station=10,place_id=1}) an query.record().value(0).toInt()!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        query2.prepare("UPDATE vpj.robot SET jobtype_id = 2 WHERE robot_id = :robot_id");
        query2.bindValue(":robot_id", query.record().value(0).toInt());
        database->Exec(&query2);
    }
}

void Dispatcher::charging()
{
    QSqlQuery query, query2, query3;
    //suche freie Ladestationen
    query.prepare("SELECT place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 0 AND maintenance = 0;");
    database->Exec(&query);
    //suche freie Roboter mit geringstem Battery Zustand, aber mindestens Batteriezustand < 30
    query2.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE battery_level < 30 AND maintenance = 0 AND state_id IN (0, 1, 2, 6) ORDER BY battery_level ASC");
    database->Exec(&query2);
    while (query.next()) // solange eine freie Ladestation vorhanden
    {
        //qDebug() << "place_id" << query.record().value(0).toInt();
        if (query2.next()) // wenn auch ein Roboter da ist, der geladen werden muss
        {
            //qDebug() << "robot_id" << query2.record().value(0).toInt() << ", state_id" << query.record().value(1).toInt();
            if (query2.record().value(1).toInt() == 0) // wenn freie Ladestation und freier Roboter
            {
                //qDebug() << "Ladeauftrag erteilen";
            // publish Ladeauftrag (jobtype =1, destination{station=9,place_id=query.record().value(0).toInt()}) an query2.record().value(0).toInt()!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

            //stationsplatz reservieren
                query3.prepare("UPDATE vpj.station_place SET state_id = 2 WHERE station_id = 9 AND place_id = :place_id;");
                query3.bindValue(":place_id", query.record().value(0).toInt());
                database->Exec(&query3);
            //robotertabelle aktualisieren
                query3.prepare("UPDATE vpj.robot SET jobtype_id = 1 WHERE  robot_id = :robot_id;");
                query3.bindValue(":robot_id", query2.record().value(0).toInt());
                database->Exec(&query3);
            }
        }
    }
}

void Dispatcher::breakjob()
{
//Pause - nach Transport
    QSqlQuery query;
    query.prepare("SELECT station.station_id, robot.station_place_id, robot.robot_id FROM vpj.robot JOIN vpj.station_place ON robot.station_place_id = station_place.station_place_id JOIN vpj.station ON station_place.station_id = station.station_id WHERE robot.state_id = 0 AND station.state_id = 1;");
    database->Exec(&query);
    while (query.next())
    {
        if (query.record().value(0).toInt() < 9)
        {
            this->publishbreak(query.record().value(0).toInt(),query.record().value(1).toInt(), query.record().value(2).toInt());
        }

    }
//Pause - nach Laden
    query.prepare("SELECT robot.station_place_id, robot.robot_id FROM vpj.robot JOIN vpj.station_place ON robot.station_place_id = station_place.station_place_id WHERE robot.state_id = 0 AND station_place.state_id = 1 AND (station_place.station_place_id = 25 OR station_place.station_place_id = 26);");
    database->Exec(&query);
    while (query.next())
    {
       this->publishbreak(9, query.record().value(0).toInt(), query.record().value(1).toInt());
    }

}

void Dispatcher::publishbreak(int station_id, int station_place_id, int robot_id)
{
    QSqlQuery query;

    // publish Pausenauftrag (jobtype = 3, destination{station=10,place_id=1}) an robot_id!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (station_id != 9) //Pause nach Transport
    {
       //qDebug() << "nach trans: publish stat_id " << station_id << ", place " << station_place_id << ", rob_id " << robot_id;
       //Station aktualisieren
       query.prepare("UPDATE vpj.station SET clearing_time = NOW() WHERE station_id = :station_id;");
       query.bindValue(":station_id", station_id);
       database->Exec(&query);
    }
    else //Pause nach Laden
    {
       //qDebug() << "nach load: publish stat_id " << station_id << ", place " << station_place_id << ", rob_id " << robot_id;
       //Stationsplatz aktualisieren
       query.prepare("UPDATE vpj.station_place SET clearing_time = NOW() WHERE station_place_id = :station_place_id;");
       query.bindValue(":station_place_id", station_place_id);
       database->Exec(&query);
    }

    //robotertabelle aktualisieren
    query.prepare("UPDATE vpj.robot SET jobtype_id = 3 WHERE robot_id = :robot_id;");
    query.bindValue(":robot_id", robot_id);
    database->Exec(&query);
}


void Dispatcher::transport()
{
    QSqlQuery query, query2, query3, query4, query5, query6, query7;
    int transportJobAllocated = 0;

    int workpiece_id, start_station_place_id, destination_station_id,  step_duration;
    int alternative_destination_station_id = 0;
    int destination_station_place_id = -1;
    int destination_place_id = -1;
    int start_station_id = -1;
    int start_place_id = -1;


    //sind freie Roboter vorhanden?
    query.prepare("SELECT robot_id FROM vpj.robot WHERE state_id = 0 AND jobtype_id = 3;");
    database->Exec(&query);
    if (query.next()) //wenn ein Roboter vorhanden ist
    {
        //nächster Bearbeitungsschritt:
        //Bestimmung des nächsten Schrittes für Werkstücke bei denen der Produktionsschritt abgeschlossen ist

        query2.prepare("SELECT workpiece_id, step_id, production_process_id, station_place_id FROM vpj.workpiece WHERE workpiece_state_id = 1 AND production_order_id IS not NULL ORDER BY TIMESTAMP ASC;");
        database->Exec(&query2);
        while (query2.next() && transportJobAllocated == 0)
        {
            workpiece_id = query2.record().value(0).toInt();
            start_station_place_id = query2.record().value(3).toInt();

            query3.prepare("SELECT sequence FROM vpj.production_step WHERE step_id = :step_id AND production_process_id = :production_process_id;");
            query3.bindValue(":step_id", query2.record().value(1).toInt());
            query3.bindValue(":production_process_id", query2.record().value(2).toInt());
            database->Exec(&query3);
            query3.next();

            //suche nächsten Schritt
            query4.prepare("SELECT step_id FROM vpj.production_step WHERE sequence = :sequence AND production_process_id = :production_process_id;");
            query4.bindValue(":sequence", query3.record().value(0).toInt() +1);
            query4.bindValue(":production_process_id", query2.record().value(2).toInt());
            database->Exec(&query4);
            query4.next();

            if (query4.record().value(0).toInt() != 5) // wenn nächste Schritt_id eine Bearbeitungsmaschine
            {
                query5.prepare("SELECT station_id, step_duration FROM vpj.production_step WHERE sequence = :sequence AND production_process_id = :production_process_id;");
                query5.bindValue(":sequence", query3.record().value(0).toInt() +1);
                query5.bindValue(":production_process_id", query2.record().value(2).toInt());
                database->Exec(&query5);
                query5.next();
                destination_station_id = query5.record().value(0).toInt();
                step_duration = query5.record().value(1).toInt();
            }
            if (query4.record().value(0).toInt() == 5) // wenn nächste Schritt_id eine Fertigteillager
            {
                destination_station_id = 7;
                alternative_destination_station_id = 8;
                step_duration = 0;
            }
            qDebug() << "workpiece_id" << workpiece_id << ", start_station_place_id" << start_station_place_id << ", destination_station_id" << destination_station_id << ", alternative_destination_station_id" << alternative_destination_station_id << ", step_duration" << step_duration;

            //nächste Werkstücke die bearbeitet werden können

            //gibt es einen nächsten freien Platz bei der Zielstation?
            query4.prepare("SELECT station_place_id, place_id FROM vpj.station_place WHERE station_id = :station_id AND state_id = 0 AND place_id != 1;");
            query4.bindValue(":station_id", destination_station_id);
            database->Exec(&query4);
            if (query4.next())
            {
                destination_station_place_id = query4.record().value(0).toInt();
                destination_place_id = query4.record().value(1).toInt();
            }
            else if (alternative_destination_station_id != 0)
            {
                query4.prepare("SELECT station_place_id, place_id FROM vpj.station_place WHERE station_id = :station_id AND state_id = 0 AND place_id != 1;");
                query4.bindValue(":station_id", alternative_destination_station_id);
                database->Exec(&query4);
                if (query4.next())
                {
                    destination_station_place_id = query4.record().value(0).toInt();
                    destination_place_id = query4.record().value(1).toInt();
                    destination_station_id = alternative_destination_station_id;
                }
            }

            qDebug() << "destination_station_place_id" << destination_station_place_id << ", destination_place_id" << destination_place_id << ", destination_station_id" << destination_station_id;
            if (destination_place_id != -1)
            {
                //steht kein Roboter an der aktuellen Station?
                query6.prepare("SELECT station.station_id, station_place.place_id FROM vpj.station INNER JOIN vpj.station_place ON station.station_id = station_place.station_id WHERE station_place.station_place_id = :station_place_id AND station.state_id = 0;");
                query6.bindValue(":station_place_id", start_station_place_id);
                database->Exec(&query6);
                if (query6.next())
                {
                    start_station_id = query6.record().value(0).toInt();
                    start_place_id = query6.record().value(1).toInt();
                    qDebug() << "start_station_id" << start_station_id << "start_place_id" << start_place_id;

                    //steht kein Roboter an der nächsten Station & die nächste Station muss nicht in Wartung?
                    query7.prepare("SELECT station_id FROM vpj.station WHERE station_id = :station_id AND state_id = 0 AND maintenance = 0; ");
                    query7.bindValue(":station_id", destination_station_id);
                    database->Exec(&query7);
                    if (query7.next())
                    {
                        //ZUTEILUNGSSTRATEGIE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        qDebug() << "übergabe zuteilungsstrategie" << query7.record().value(0).toInt();
                        transportJobAllocated = 1; //nach Zuteilung muss das erfolgen, abbruch bedingung
                    }
                }
            }
        }


/*-- 1.4 Dispatcher -Transport

-- Zuteilungsstrategie
-- position des Werkstücks
-- SELECT place_position_x, place_position_y FROM vpj.station_place INNER JOIN vpj.workpiece ON station_place.station_place_id = workpiece.station_place_id WHERE workpiece.workpiece_id = 1;
-- position des Roboters
-- SELECT robot_id, robot_position_x, robot_position_y FROM vpj.robot WHERE jobtype_id = 3;
-- nach wahl hier bspw. robot 1 -> Zuteilung schreiben in DB
-- UPDATE vpj.robot SET jobtype_id = 0 WHERE robot_id = 1;
-- nach wahl (erster in der liste) hier Ziel station_place = 8
-- UPDATE vpj.workpiece SET current_step_duration = 60, destination_station_place_id =  8, workpiece_state_id = 3, robot_id = 1, station_place_id = 2, step_id = 1 WHERE workpiece_id = 1;
-- stationen reservieren hier start = 1 und ziel = 3
-- UPDATE vpj.station SET state_id = 2 WHERE station_id = 1 OR station_id = 3;
-- stationsplatz reservieren, hier ziel = 8
-- UPDATE vpj.station_place SET state_id = 2 WHERE station_place_id = 8;
*/
    }

}
