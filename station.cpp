#include "station.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlIndex>
#include <QDateTime>

Station::Station()
{
    database = new Database();
    database->Connect();
}

Station::~Station()
{
    database->Disconnect();
}

void Station::updateStation()
{
    stationrelease();
    maintenanceChargingStation();
    workpieceProcessing();
}

void Station::stationrelease() //Stations-/Platzfreigabe nach dem ein Roboter aus dem Weg gefahren ist
{
    QSqlQuery query, query2;
    int blocking_time = 10;
    QDateTime currentDateTime = QDateTime::currentDateTime();

    //Transportstationsfreigabe wenn Roboter weg gefahren sind
    query.prepare("SELECT clearing_time, station_id FROM vpj.station WHERE state_id = 1 AND clearing_time IS NOT NULL;");
    database->Exec(&query);
    while (query.next())
    {
        QVariant clearingTimeVariant = query.record().value(0);
        if (clearingTimeVariant.isValid() && clearingTimeVariant.canConvert<QDateTime>()) //überprüft ob clearingTime convertiert werden kann, ohne gab es sonst Fehler
        {
            QDateTime clearingTime = clearingTimeVariant.toDateTime();
            int timeDifference = clearingTime.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
            if (timeDifference > blocking_time) //wenn Zeitdifferenz > als Sperrzeit
            {
                query2.prepare("UPDATE vpj.station SET state_id = 0, clearing_time = NULL WHERE station_id = :station_id");
                query2.bindValue(":station_id", query.record().value(1).toInt());
                database->Exec(&query2);
            }
        }
    }

    //Ladestationsplatzfreigabe wenn Roboter weg gefahren sind
    query.prepare("SELECT clearing_time, station_place_id FROM vpj.station_place WHERE state_id = 1 AND clearing_time IS NOT NULL;");
    database->Exec(&query);
    while (query.next())
    {
        QVariant clearingTimeVariant = query.record().value(0);
        if (clearingTimeVariant.isValid() && clearingTimeVariant.canConvert<QDateTime>()) //überprüft ob clearingTime convertiert werden kann, ohne gab es sonst Fehler
        {
            QDateTime clearingTime = clearingTimeVariant.toDateTime();
            int timeDifference = clearingTime.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
            if (timeDifference > blocking_time) //wenn Zeitdifferenz > als Sperrzeit
            {
                //Ladestationsplätze + Historie aktualisieren
                query2.prepare("UPDATE vpj.station_place SET state_id = 0, clearing_time = NULL WHERE station_place_id = :station_place_id;");
                query2.bindValue(":station_place_id", query.record().value(1).toInt());
                database->Exec(&query2);
                query2.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_place_id = :station_place_id; ");
                query2.bindValue(":station_place_id", query.record().value(1).toInt());
                database->Exec(&query2);
            }
        }
    }
}


void Station::maintenanceChargingStation()
{
    //Ladestationen in Wartung schicken
    QSqlQuery query, query2;
    query.prepare("SELECT station_place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 0 AND maintenance = 1;");
    database->Exec(&query);
    while (query.next())
    {
        //Aktualisierung Stationsplatz + History (Status = inaktiv (in Wartung))
        query2.prepare("UPDATE vpj.station_place SET state_id = 3 WHERE station_place_id = :station_place_id;");
        query2.bindValue(":station_place_id", query.record().value(0).toInt());
        database->Exec(&query2);
        query2.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_place_id = :station_place_id; ");
        query2.bindValue(":station_place_id", query.record().value(0).toInt());
        database->Exec(&query2);
    }

    //Ladestationen aus Wartung herausholen
    query.prepare("SELECT station_place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 3 AND maintenance = 0;");
    database->Exec(&query);
    while (query.next())
    {
        //Aktualisierung Stationsplatz + History (Status = frei)
        query2.prepare("UPDATE vpj.station_place SET state_id = 0 WHERE station_place_id = :station_place_id;");
        query2.bindValue(":station_place_id", query.record().value(0).toInt());
        database->Exec(&query2);
        query2.prepare("INSERT INTO vpj.station_place_history (state_id, station_place_id) SELECT state_id, station_place_id FROM vpj.station_place WHERE station_place_id = :station_place_id; ");
        query2.bindValue(":station_place_id", query.record().value(0).toInt());
        database->Exec(&query2);
    }
}

void Station::workpieceProcessing() //Überprüfung der Werkstückbearbeitungszeit
{
    QSqlQuery query, query2, query3;
    QDateTime currentDateTime = QDateTime::currentDateTime();

    query.prepare("SELECT workpiece_id, step_id FROM vpj.workpiece INNER JOIN vpj.station_place ON station_place.station_place_id = workpiece.station_place_id WHERE station_place.state_id = 1 AND workpiece.workpiece_state_id = 2");
    database->Exec(&query);
    while (query.next())
    {
        if (query.record().value(1).toInt() == 5) //wenn das Werkstück im Fertigteillager angekommen ist
        {
            //Aktualisierung Werkstücktabelle + Historie (status = fertig produziert)
            query2.prepare("UPDATE vpj.workpiece SET workpiece_id = 0 WHERE workpiece_id = :workpiece_id;");
            query2.bindValue(":workpiece_id", query.record().value(0).toInt());
            database->Exec(&query2);
            query2.prepare("INSERT INTO vpj.workpiece_history (rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id) SELECT rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id; ");
            query2.bindValue(":workpiece_id", query.record().value(0).toInt());
            database->Exec(&query2);
        }
        else
        {
            query2.prepare("SELECT timestamp, current_step_duration FROM vpj.workpiece WHERE workpiece_id = :workpiece_id;");
            query2.bindValue(":workpiece_id", query.record().value(0).toInt());
            database->Exec(&query2);
            query2.next();
            QVariant timestampVariant = query2.record().value(0);
            if (timestampVariant.isValid() && timestampVariant.canConvert<QDateTime>()) //überprüft ob timestamp convertiert werden kann
            {
                QDateTime timestamp = timestampVariant.toDateTime();
                int timeDifference = timestamp.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
                if (timeDifference > query2.record().value(1).toInt()) //wenn Zeitdifferenz > aktuelle Schrittdauer (Bearbeitungszeit)
                {
                    //Werkstücktabelle + Historie aktualisieren (status=fertiger Schritt)
                    query3.prepare("UPDATE vpj.workpiece SET workpiece_state_id = 1 WHERE workpiece_id = :workpiece_id;");
                    query3.bindValue(":workpiece_id", query.record().value(0).toInt());
                    database->Exec(&query3);
                    query3.prepare("INSERT INTO vpj.workpiece_history (rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id) SELECT rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id; ");
                    query3.bindValue(":workpiece_id", query.record().value(0).toInt());
                    database->Exec(&query3);
                }
            }
        }
    }
}
