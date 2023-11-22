#include "workpiece.h"

Workpiece::Workpiece(QObject *parent)
    : QObject{parent}
{
    database = new Database("Workpiece");
    database->connect();
}

Workpiece::~Workpiece()
{
    database->disconnect();
}

void Workpiece::updateOrder()
{
    this->orderAllocation();
    this->orderAdministration();
}

//Werkstück - Auftragszuordnung
void Workpiece::orderAllocation()
{
    // Suche nach nicht zugeordnetem Werkstück zu Auftrag, mit ältestem Zeitstempel, die nicht fehlerhaft sind
    QSqlQuery query(database->db());
    query.prepare("SELECT workpiece_id FROM vpj.workpiece WHERE production_order_id IS NULL AND workpiece_state_id != 6 ORDER BY TIMESTAMP ASC LIMIT 1");
    query.exec();
    if(query.next())
    {
        int workpieceId = query.record().value(0).toInt();
        //qDebug() << "Werkstück" << workpieceId << "ist keinem Auftrag zugeordnet";
        //Suche nach ältestem Auftrag, für den noch werkstücke bearbeitet werden müssen
        QSqlQuery query2(database->db());
        query2.prepare("SELECT production_order_id FROM vpj.production_order WHERE assigned_workpieces < number_of_pieces ORDER BY TIMESTAMP ASC LIMIT 1");
        query2.exec();
        if(query2.next())
        {
            int productionOrderId = query2.record().value(0).toInt();
            qDebug() << "Für Produktionsauftrag" << productionOrderId << "müssen noch Werkstücke bearbeitet werden";
            //aktualisiere Produktionsauftragstabelle (zugeteilte Werkstücke+=1 und auftragsstatus = "in Bearbeitung")
            QSqlQuery query3(database->db());
            query3.prepare("UPDATE vpj.production_order SET assigned_workpieces = assigned_workpieces +1, workpiece_state_id = 2 WHERE production_order_id = :production_order_id;");
            query3.bindValue(":production_order_id", productionOrderId);
            query3.exec();
            //suche nach fertigungsablauf_id
            query3.prepare("SELECT production_process_id FROM vpj.production_order WHERE production_order_id = :production_order_id");
            query3.bindValue(":production_order_id", productionOrderId);
            query3.exec();
            query3.next();
            int productionProcessId = query3.record().value(0).toInt();
            qDebug() << "Werkstück" << workpieceId << "wird dem Auftrag" << productionOrderId << "zugewiesen";
            //Werkstücktabelle + Historie aktualisieren (Zuordnung des Werkstückes zum Auftrag)
            query3.prepare("UPDATE vpj.workpiece SET production_order_id = :production_order_id, step_id = 0, production_process_id = :production_process_id WHERE workpiece_id = :workpiece_id;");
            query3.bindValue(":production_order_id", productionOrderId);
            query3.bindValue(":production_process_id", productionProcessId);
            query3.bindValue(":workpiece_id", workpieceId);
            query3.exec();
            database->updateWorkpieceHistory(workpieceId);
        }
    }
}

//Auftragsverwaltung
void Workpiece::orderAdministration()
{
    QSqlQuery query(database->db());
    // haben alle Werkstücke zu einem Auftrag (in Bearbeitung) den status fertig produziert oder ausgeliefert?
    query.prepare("SELECT COUNT(wp.workpiece_id), wp.production_order_id, production_order.assigned_workpieces FROM vpj.workpiece AS wp INNER JOIN vpj.production_order ON production_order.production_order_id = wp.production_order_id WHERE production_order.workpiece_state_id = 2 AND production_order.number_of_pieces = production_order.assigned_workpieces AND (wp.workpiece_state_id = 0 OR wp.workpiece_state_id = 4);");
    query.exec();
    while (query.next())
    {
        int workpieceCount = query.record().value(0).toInt();
        int productionOrderId = query.record().value(1).toInt();
        int assignedWorkpieces = query.record().value(2).toInt();
        //qDebug() << "in Bearbeitung -> fertig produziert" << workpieceCount << productionOrderId << assignedWorkpieces;
        if (workpieceCount != 0 && workpieceCount == assignedWorkpieces)
        {
            // aktualisiere den Auftrag (status = fertig produziert)
            qDebug() << workpieceCount << "Werkstücke fertig produziert";
            QSqlQuery query2(database->db());
            query2.prepare("UPDATE vpj.production_order SET workpiece_state_id = 0 WHERE production_order_id = :production_order_id");
            query2.bindValue(":production_order_id", productionOrderId);
            query2.exec();
        }
    }
    // haben alle Werkstücke zu einem Auftrag (fertig produziert) den status ausgeliefert?
    query.prepare("SELECT COUNT(wp.workpiece_id), wp.production_order_id, production_order.assigned_workpieces FROM vpj.workpiece AS wp INNER JOIN vpj.production_order ON production_order.production_order_id = wp.production_order_id WHERE production_order.workpiece_state_id = 0 AND production_order.number_of_pieces = production_order.assigned_workpieces AND wp.workpiece_state_id = 4;");
    query.exec();
    while (query.next())
    {
        int workpieceCount = query.record().value(0).toInt();
        int productionOrderId = query.record().value(1).toInt();
        int assignedWorkpieces = query.record().value(2).toInt();
        //qDebug() << "fertig produziert -> ausgeliefert" << workpieceCount << productionOrderId << assignedWorkpieces;
        if (workpieceCount != 0 && workpieceCount == assignedWorkpieces)
        {
            // aktualisiere den Auftrag (status = ausgeliefert)
            qDebug() << workpieceCount << "Werkstücke ausgeliefert";
            QSqlQuery query2(database->db());
            query2.prepare("UPDATE vpj.production_order SET workpiece_state_id = 4, delivery_time = NOW() WHERE production_order_id = :production_order_id;");
            query2.bindValue(":production_order_id", productionOrderId);
            query2.exec();
        }
    }
}
