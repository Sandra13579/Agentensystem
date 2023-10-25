#include "station.h"

Station::Station(QObject *parent)
    : QObject{parent}
{
    database = new Database("Station");
    database->Connect();
}

Station::~Station()
{
    database->Disconnect();
}

void Station::updateStation()
{
    stationRelease();
    maintenanceChargingStation();
    workpieceProcessing();
}

void Station::stationRelease() //Stations-/Platzfreigabe nach dem ein Roboter aus dem Weg gefahren ist
{
    int blockingTime = 10;
    QDateTime currentDateTime = QDateTime::currentDateTime();

    //Transportstationsfreigabe wenn Roboter weg gefahren sind
    QSqlQuery query(database->db());
    query.prepare("SELECT clearing_time, station_id FROM vpj.station WHERE state_id = 1 AND clearing_time IS NOT NULL;");
    query.exec();
    while (query.next())
    {
        QVariant clearingTimeVariant = query.record().value(0);
        if (clearingTimeVariant.isValid() && clearingTimeVariant.canConvert<QDateTime>()) //überprüft ob clearingTime convertiert werden kann, ohne gab es sonst Fehler
        {
            QDateTime clearingTime = clearingTimeVariant.toDateTime();
            int timeDifference = clearingTime.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
            if (timeDifference > blockingTime) //wenn Zeitdifferenz > als Sperrzeit
            {
                QSqlQuery query2(database->db());
                query2.prepare("UPDATE vpj.station SET state_id = 0, clearing_time = NULL WHERE station_id = :station_id");
                query2.bindValue(":station_id", query.record().value(1).toInt());
                query2.exec();
            }
        }
    }

    //Ladestationsplatzfreigabe wenn Roboter weg gefahren sind
    query.prepare("SELECT clearing_time, station_place_id FROM vpj.station_place WHERE state_id = 1 AND clearing_time IS NOT NULL;");
    query.exec();
    while (query.next())
    {
        QVariant clearingTimeVariant = query.record().value(0);
        if (clearingTimeVariant.isValid() && clearingTimeVariant.canConvert<QDateTime>()) //überprüft ob clearingTime convertiert werden kann, ohne gab es sonst Fehler
        {
            QDateTime clearingTime = clearingTimeVariant.toDateTime();
            int timeDifference = clearingTime.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
            if (timeDifference > blockingTime) //wenn Zeitdifferenz > als Sperrzeit
            {
                //Ladestationsplätze + Historie aktualisieren
                QSqlQuery query2(database->db());
                query2.prepare("UPDATE vpj.station_place SET state_id = 0, clearing_time = NULL WHERE station_place_id = :station_place_id;");
                query2.bindValue(":station_place_id", query.record().value(1).toInt());
                query2.exec();
                database->updateStationPlaceHistory(query.record().value(1).toInt());
            }
        }
    }
}

void Station::maintenanceChargingStation()
{
    //Ladestationen in Wartung schicken
    QSqlQuery query(database->db());
    query.prepare("SELECT station_place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 0 AND maintenance = 1;");
    query.exec();
    while (query.next())
    {
        //Aktualisierung Stationsplatz + History (Status = inaktiv (in Wartung))
        database->updateStationPlaceState(query.record().value(0).toInt(), State::Inactive);
        database->updateStationPlaceHistory(query.record().value(0).toInt());
    }

    //Ladestationen aus Wartung herausholen
    query.prepare("SELECT station_place_id FROM vpj.station_place WHERE (station_place_id = 25 OR station_place_id = 26) AND state_id = 3 AND maintenance = 0;");
    query.exec();
    while (query.next())
    {
        //Aktualisierung Stationsplatz + History (Status = frei)
        database->updateStationPlaceState(query.record().value(0).toInt(), State::Available);
        database->updateStationPlaceHistory(query.record().value(0).toInt());
    }
}

void Station::workpieceProcessing() //Überprüfung der Werkstückbearbeitungszeit
{
    QDateTime currentDateTime = QDateTime::currentDateTime();

    QSqlQuery query(database->db());
    query.prepare("SELECT workpiece_id, step_id FROM vpj.workpiece INNER JOIN vpj.station_place ON station_place.station_place_id = workpiece.station_place_id WHERE station_place.state_id = 1 AND workpiece.workpiece_state_id = 2");
    query.exec();
    while (query.next())
    {
        if (query.record().value(1).toInt() == 5) //wenn das Werkstück im Fertigteillager angekommen ist
        {
            //Aktualisierung Werkstücktabelle + Historie (status = fertig produziert)
            QSqlQuery query2(database->db());
            query2.prepare("UPDATE vpj.workpiece SET workpiece_state_id = 0 WHERE workpiece_id = :workpiece_id;");
            query2.bindValue(":workpiece_id", query.record().value(0).toInt());
            query2.exec();
            database->updateWorkpieceHistory(query.record().value(0).toInt());
        }
        else
        {
            QSqlQuery query2(database->db());
            query2.prepare("SELECT timestamp, current_step_duration FROM vpj.workpiece WHERE workpiece_id = :workpiece_id;");
            query2.bindValue(":workpiece_id", query.record().value(0).toInt());
            query2.exec();
            query2.next();
            QVariant timestampVariant = query2.record().value(0);
            if (timestampVariant.isValid() && timestampVariant.canConvert<QDateTime>()) //überprüft ob timestamp convertiert werden kann
            {
                QDateTime timestamp = timestampVariant.toDateTime();
                int timeDifference = timestamp.secsTo(currentDateTime); // Zeitunterschied in Sekunden berechnen
                if (timeDifference > query2.record().value(1).toInt()) //wenn Zeitdifferenz > aktuelle Schrittdauer (Bearbeitungszeit)
                {
                    //Werkstücktabelle + Historie aktualisieren (status=fertiger Schritt)
                    QSqlQuery query3(database->db());
                    query3.prepare("UPDATE vpj.workpiece SET workpiece_state_id = 1 WHERE workpiece_id = :workpiece_id;");
                    query3.bindValue(":workpiece_id", query.record().value(0).toInt());
                    query3.exec();
                    database->updateWorkpieceHistory(query.record().value(0).toInt());
                }
            }
        }
    }
}
