#include "workpiece.h"
#include "database.h"

#include <QSqlQuery>
#include <QSqlIndex>

Workpiece::Workpiece()
{
    database = new Database();
    database->Connect();
}

Workpiece::~Workpiece()
{
    database->Disconnect();
}

void Workpiece::updateOrder()
{
    this->orderAllocation();
    this->orderAdministration();
}

//Werkstück - Auftragszuordnung
void Workpiece::orderAllocation()
{
    // Such nach freiem, nicht zugeordnetem Werkstück zu Auftrag, mit ältestem Zeitstempel
    QSqlQuery query, query2, query3, query4;
    query.prepare("SELECT workpiece_id FROM vpj.workpiece WHERE production_order_id IS NULL ORDER BY TIMESTAMP ASC LIMIT 1");
    database->Exec(&query);
    if(query.next())
    {
        //Such nach ältestem Auftrag, für den noch werkstücke bearbeitet werden müssen
        query2.prepare("SELECT production_order_id FROM vpj.production_order WHERE assigned_workpieces < number_of_pieces ORDER BY TIMESTAMP ASC LIMIT 1;");
        database->Exec(&query2);
        if(query2.next())
        {
            //aktualisiere Produktionsauftragstabelle (zugeteilte Werkstücke+=1 und auftragsstatus = "in Bearbeitung")
            query3.prepare("UPDATE vpj.production_order SET assigned_workpieces = assigned_workpieces +1, workpiece_state_id = 2 WHERE production_order_id = :production_order_id;");
            query3.bindValue(":production_order_id", query2.record().value(0).toInt());
            database->Exec(&query3);
            //suche nach fertigungsablauf_id
            query3.prepare("SELECT production_process_id FROM vpj.production_order WHERE production_order_id = :production_order_id;");
            query3.bindValue(":production_order_id", query2.record().value(0).toInt());
            database->Exec(&query3);
            query3.next();
            //Werkstücktabelle + Historie aktualisieren (Zuordnung des Werkstückes zum Auftrag)
            query4.prepare("UPDATE vpj.workpiece SET production_order_id = :production_order_id, step_id = 0, production_process_id = :production_process_id WHERE workpiece_id = :workpiece_id;");
            query4.bindValue(":production_order_id", query2.record().value(0).toInt());
            query4.bindValue(":production_process_id", query3.record().value(0).toInt());
            query4.bindValue(":workpiece_id", query.record().value(0).toInt());
            database->Exec(&query4);
            query4.prepare("INSERT INTO vpj.workpiece_history (rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id) SELECT rfid, current_step_duration, workpiece_state_id, workpiece_id, robot_id, station_place_id, production_order_id, step_id, production_process_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id; ");
            query4.bindValue(":workpiece_id", query.record().value(0).toInt());
            database->Exec(&query4);
        }
    }
}

//Auftragsverwaltung
void Workpiece::orderAdministration()
{
    QSqlQuery query, query2;

    // haben alle Werkstücke zu einem Auftrag (in Bearbeitung) den status fertig produziert oder ausgeliefert?
    query.prepare("SELECT COUNT(wp.workpiece_id), wp.production_order_id, production_order.assigned_workpieces FROM vpj.workpiece AS wp INNER JOIN vpj.production_order ON production_order.production_order_id = wp.production_order_id WHERE production_order.workpiece_state_id = 2 AND production_order.number_of_pieces = production_order.assigned_workpieces AND (wp.workpiece_state_id = 0 OR wp.workpiece_state_id = 4);");
    database->Exec(&query);
    while (query.next())
    {
        //qDebug() << "in Bearbeitung -> fertig produziert" << query.record().value(0).toInt() << query.record().value(1).toInt() << query.record().value(2).toInt();
        if (query.record().value(0).toInt() == query.record().value(2).toInt() && query.record().value(0).toInt() != 0)
        {
            // aktualisiere den Auftrag (status = fertig produziert)
            query2.prepare("UPDATE vpj.production_order SET workpiece_state_id = 0 WHERE production_order_id = :production_order_id");
            query2.bindValue(":production_order_id", query.record().value(1).toInt());
            database->Exec(&query2);
        }
    }
    // haben alle Werkstücke zu einem Auftrag (fertig produziert) den status ausgeliefert?
    query.prepare("SELECT COUNT(wp.workpiece_id), wp.production_order_id, production_order.assigned_workpieces FROM vpj.workpiece AS wp INNER JOIN vpj.production_order ON production_order.production_order_id = wp.production_order_id WHERE production_order.workpiece_state_id = 0 AND production_order.number_of_pieces = production_order.assigned_workpieces AND wp.workpiece_state_id = 4;");
    database->Exec(&query);
    while (query.next())
    {
        qDebug() << "fertig produziert -> ausgeliefert" << query.record().value(0).toInt() << query.record().value(1).toInt() << query.record().value(2).toInt();
        if (query.record().value(0).toInt() == query.record().value(2).toInt() && query.record().value(0).toInt() != 0)
        {
            // aktualisiere den Auftrag (status = ausgeliefert)
            query2.prepare("UPDATE vpj.production_order SET workpiece_state_id = 4, delivery_time = NOW() WHERE production_order_id = :production_order_id;");
            query2.bindValue(":production_order_id", query.record().value(1).toInt());
            database->Exec(&query2);
        }
    }
}
