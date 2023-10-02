#include "robot.h"

Robot::Robot(QObject *parent)
    : QObject{parent}
{
    m_database = new Database("Robot");
    m_database->Connect();

    for (int i = 0; i < 5; ++i)
    {
        m_robotStates.append(State::Initial);
        m_robotPlaces.append({ 0, 0 });
    }

    for (int i = 0; i < 3; ++i) {
        m_chargingStationStates.append(State::Initial);
    }
}

void Robot::updateRobot()
{
    //this->transport();
    this->charging();
    this->pause();
    this->maintenance();
}

Robot::~Robot()
{
    m_database->Disconnect();
}

//Update robot states from GW2 (MQTT)
void Robot::updateRobotStatus(int robotId, State state, Place place)
{
    m_robotStates[robotId] = state;
    m_robotPlaces[robotId] = place;
}

//Update charging station states from GW4 (MQTT)
void Robot::updateChargingStationStatus(int stationId, State state)
{
    m_chargingStationStates[stationId] = state;
}

//Update robots state in DB
void Robot::updateRobotDatabase(int robotId, State state)
{
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.robot SET state_id = :state_id, timestamp = NOW() WHERE robot_id = :robot_id AND state_id <> :state_id");
    query.bindValue(":robot_id", robotId);
    query.bindValue(":state_id", static_cast<int>(state));
    query.exec();
}

//Update robots station place in DB
void Robot::updateRobotDatabase(int robotId, State state, Place place)
{
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.robot SET state_id = :state_id, station_place_id = (SELECT station_place_id FROM vpj.station_place WHERE station_id = :station_id AND place_id = :place_id), timestamp = NOW() WHERE robot_id = :robot_id");
    query.bindValue(":robot_id", robotId);
    query.bindValue(":state_id", static_cast<int>(state));
    query.bindValue(":station_id", place.stationId);
    query.bindValue(":place_id", place.placeId);
    query.exec();
}

void Robot::maintenance()
{
    QSqlQuery query(m_database->db());
    //Roboter raussuchen, die in Wartung sind und einen Wartungsauftrag besitzen
    query.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE maintenance = 1 AND jobtype_id = 2 AND state_id <> 3");
    query.exec();
    while (query.next())
    {
        //Solange Roboter in Betrieb vorhanden sind
        int robotId = query.record().value(0).toInt();
        State robotState = static_cast<State>(query.record().value(1).toInt());

        //Status = reserviert (Gewerk 2) & status = frei (DB) -> Roboter fährt zum Wartungsplatz
        if (m_robotStates[robotId] == State::Reserved && robotState == State::Available)
        {
            updateRobotDatabase(robotId, State::Reserved);
            qDebug() << "Roboter" << robotId << "fährt zur Wartung";
        }

        //Status = inaktiv (Gewerk 2) & status = reserviert (DB) -> Roboter ist am Wartungsplatz angekommen
        if (m_robotStates[robotId] == State::Inactive && robotState == State::Reserved)
        {
            updateRobotDatabase(robotId, State::Inactive, m_robotPlaces[robotId]);
            qDebug() << "Roboter" << robotId << "befindet sich in Wartung";
        }
    }
}

void Robot::pause()
{
    QSqlQuery query(m_database->db());
    //Roboter raussuchen, die nicht in Wartung sind und einen Pauseauftrag besitzen
    query.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE maintenance = 0 AND jobtype_id = 3");
    query.exec();
    while (query.next())
    {
        //Solange Roboter in Betrieb vorhanden sind
        int robotId = query.record().value(0).toInt();
        State robotState = static_cast<State>(query.record().value(1).toInt());

        //Status = reserviert (Gewerk 2) & status = frei (DB) -> Stationsplatz aktualisieren ((10, 1) = 27), um Pausenauftrag abzuschließen
        if (m_robotStates[robotId] == State::Reserved && robotState == State::Available)
        {
            updateRobotDatabase(robotId, State::Available, m_robotPlaces[robotId]);
        }
    }
}

void Robot::transport()
{
    QSqlQuery query(m_database->db());
    //Roboter raussuchen, die einen Transportauftrag besitzen
    query.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE jobtype_id = 0");
    query.exec();
    while (query.next())
    {
        //Solange Roboter in Betrieb vorhanden sind
        int robotId = query.record().value(0).toInt();
        State robotState = static_cast<State>(query.record().value(1).toInt());

        //Roboterstatus DB = frei & Roboterstatus Gewerk 2 = reserviert -> Roboter hat Auftrag erhalten
        if (robotState == State::Available && m_robotStates[robotId] == State::Reserved)
        {
            //Roboter reservieren
            updateRobotDatabase(robotId, State::Reserved);
            qDebug() << "Roboter" << robotId << "ist reserviert";
        }

        //Roboterstatus DB = reserviert & Roboterstatus Gewerk2 = belegt -> Roboter transportiert Werkstück
        if (robotState == State::Reserved && m_robotStates[robotId] == State::Assigned)
        {
            //Roboter hat Werkstück -> belegt
            //ToDo
            //update... übermittelten Stationsplatz von GW2 in roboterDB schreiben
            updateRobotDatabase(robotId, State::Assigned);
            qDebug() << "Roboter" << robotId << "ist belegt.";
        }

        //Roboterstatus DB = belegt & Roboterstatus Gewerk2 = bereit zum Lesen -> Roboter steht mit Werkstück am RFID Reader
        if (robotState == State::Assigned && m_robotStates[robotId] == State::ReadyForReading)
        {
            reading(robotId);
            updateRobotDatabase(robotId, State::ReadyForReading);
            qDebug() << "Roboter" << robotId << "ist bereit zum Lesen";   
        }

        //Roboterstatus DB = bereit zum Lesen & Roboterstatus Gewerk2 = belegt -> Roboter transportiert Werkstück
        if (robotState == State::ReadyForReading && m_robotStates[robotId] == State::Assigned)
        {
            checking(robotId);
            //ToDo
            //update... übermittelten Stationsplatz von GW2 in roboterDB schreiben
            updateRobotDatabase(robotId, State::Assigned);
            qDebug() << "Roboter" << robotId << "ist belegt";
        }

        //Roboterstatus DB = „belegt“ & Roboterstatus (Gewerk2) = „frei“, dann rufe auf „Transportauftragabgeschlossenfunktion“
        if (robotState == State::Assigned && m_robotStates[robotId] == State::Available)
        {
            transportFinished(robotId);
            //ToDo
            //update... übermittelten Stationsplatz von GW2 in roboterDB schreiben
            updateRobotDatabase(robotId, State::Available);
            qDebug() << "Roboter" << robotId << "ist frei";
        }
    }
}

void Robot::charging()
{
    QSqlQuery query(m_database->db());
    //Roboter raussuchen, die einen Ladeauftrag besitzen
    query.prepare("SELECT robot_id, state_id FROM vpj.robot WHERE jobtype_id = 1");
    query.exec();
    while (query.next())
    {
        //Solange Roboter in Betrieb vorhanden sind
        int robotId = query.record().value(0).toInt();
        State robotState = static_cast<State>(query.record().value(1).toInt());
        int placeId = m_robotPlaces[robotId].placeId;

        /*
            Fehler
            wenn Roboterstatus „bereit zum Laden“ (Gewerk2) & (Roboterstatus =„initial“ oder NULL) (Gewerk4)
            - dann vergleich zeitstempel Robotertabelle DB mit aktueller Zeit
            wenn Zeitdifferenz > max Verzögerungszeit
            - dann Fehler: Roboterstatus „Fehler“ in DB
        */

        //wenn Roboterstatus „bereit zum Laden“ (Gewerk2) & (Roboterstatus (Gewerk4) =„initial“ oder NULL)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Initial)
        {
            int maxDelay = 30;

            QSqlQuery query(m_database->db());
            query.prepare("SELECT TIMESTAMP FROM vpj.robot WHERE robot_id = :robot_id");
            query.bindValue(":robot_id", robotId);
            query.exec();
            if (query.next())
            {
                QVariant clearingTimeVariant = query.record().value(0);
                if (clearingTimeVariant.isValid() && clearingTimeVariant.canConvert<QDateTime>())
                {
                    QDateTime clearingTime = clearingTimeVariant.toDateTime();
                    if (clearingTime.secsTo(QDateTime::currentDateTime()) > maxDelay)
                    {
                        qDebug() << "Fehler beim Verbinden der Ladestation mit dem Roboter" << placeId;
                        updateRobotDatabase(robotId, State::Fault);
                    }
                }
            }
        }

        //wenn Roboterstatus „bereit zum Laden“ (Gewerk2) & (Roboterstatus (Gewerk4) =„initial“ oder "verbunden") und nicht Roboterstaus "frei" (DB)
        if (m_robotStates[robotId] == State::ReadyForCharging && (m_chargingStationStates[placeId] == State::Initial || m_chargingStationStates[placeId] == State::Connected) && robotState != State::Available)
        {
            //ToDo
            //update... übermittelten Stationsplatz von GW2 in roboterDB schreiben
            updateRobotDatabase(robotId, State::ReadyForCharging);
            qDebug() << "Roboter" << robotId << " lädt";
            robotState = State::ReadyForCharging;
        }

        //roboterstatus = bereit zum Laden (DB) und Ladestation = verbunden (Gewerk 4)
        if (robotState == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Connected)
        {
            //nur bei Statusänderung
            if (m_robotStates[robotId] != robotState)
            {
                updateRobotDatabase(robotId, State::Connected);
                qDebug() << "Roboter" << robotId << " ist mit Ladestation" << placeId << "verbunden";
                QSqlQuery query(m_database->db());
                //query.prepare("UPDATE vpj.station_place SET state_id = 1 WHERE station_place_id = (SELECT station_place_id FROM vpj.robot WHERE robot_id = :robot_id) AND state_id <> 1");
                query.prepare("UPDATE vpj.station_place SET state_id = 1 WHERE station_id = 9 AND place_id = :place_id AND state_id <> 1");
                query.bindValue(":place_id", placeId);
                query.exec();
                emit charge(true, placeId);
                qDebug() << "Ladevorgang wird gestartet";
            }
        }

        //jetzt wird geladen status = laden (Gewerk4) und status = bereit zum laden (Gewerk2)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Charging)
        {
            if (m_robotStates[robotId] != robotState)
            {
                updateRobotDatabase(robotId, State::Charging);
                qDebug() << "Roboter" << robotId << " lädt";
            }
        }

        //fertig geladen status = frei (Gewerk4) und status = bereit zum laden (Gewerk2)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Available)
        {
            if (m_robotStates[robotId] != robotState)
            {
                updateRobotDatabase(robotId, State::Available);
                m_chargingStationStates[placeId] = State::Initial; //<- statt publish s.o.
                qDebug() << "Ladevorgang für Roboter" << robotId << "an Ladestation" << placeId << "beendet";
            }
        }
    }
}

void Robot::reading(int robotId)
{
    QSqlQuery query(m_database->db());
    query.prepare("SELECT station_place.station_id FROM vpj.station_place INNER JOIN vpj.robot ON robot.station_place_id = station_place.station_place_id WHERE robot.robot_id = :robot_id");
    query.bindValue(":robot_id", robotId);
    query.exec();
    if (query.next())
    {
        int stationId = query.record().value(0).toInt();
        qDebug() << "Station, an dem der Roboter steht:" << stationId;
        emit rfidOn(stationId); //Turn on the RFID reader at the station
    }
}

void Robot::continueReading(int stationId, int serialNumber)
{
    qDebug() << "Continue reading sequence at station" << stationId << "with serial number" << serialNumber;
    emit rfidOff();
    QSqlQuery query(m_database->db());
    query.prepare("SELECT rfid FROM vpj.workpiece INNER JOIN vpj.station_place ON workpiece.station_place_id = station_place.station_place_id WHERE station_place.station_id = :station_id AND workpiece.workpiece_state_id = 3");
    query.bindValue(":station_id", stationId);
    query.exec();
    if (query.next())
    {
        int ratedSerialNumber = query.record().value(0).toInt();
        if (serialNumber != ratedSerialNumber)
        {
            qDebug() << "Error: Read serial number" << serialNumber << "is not equal to rated serial number" << ratedSerialNumber << "in database!";
            //Fehlermeldung in die DB schreiben!!!!!
            //TODO
            return;
        }
        qDebug() << "Rated serial number:" << ratedSerialNumber;
        QSqlQuery query2(m_database->db());
        query2.prepare("SELECT robot_id from vpj.workpiece WHERE rfid = :rfid");
        query2.bindValue(":rfid", serialNumber);
        query2.exec();
        //Determine robot number (from DB) and initiate "checkout".

        if (query2.next())
        {
            int robotNo = query2.record().value(0).toInt();
            emit check(robotNo);
        }
    }
}

void Robot::checking(int robotId)
{
    QSqlQuery query(m_database->db());
    query.prepare("SELECT checked_in, workpiece_id FROM vpj.workpiece WHERE robot_id = :robot_id");
    query.bindValue(":robot_id", robotId);
    query.exec();
    if (query.next())
    {
        int checkedIn = query.record().value(0).toInt();
        int workpieceId = query.record().value(1).toInt();
        qDebug() << "Checked in:" << checkedIn << "Workpiece ID:" << workpieceId;

        if (checkedIn == 1)
        {
            //Ausschecken
            QSqlQuery query2(m_database->db());
            query2.prepare("SELECT station.maintenance, selected_station.station_place_id, selected_station.station_id FROM vpj.station INNER JOIN ( SELECT sp.station_id, sp.station_place_id FROM vpj.workpiece AS wp INNER JOIN vpj.station_place AS sp ON wp.station_place_id = sp.station_place_id WHERE wp.robot_id = :robot_id ) AS selected_station ON station.station_id = selected_station.station_id");
            query2.bindValue(":robot_id", robotId);
            query2.exec();
            if (query2.next())
            {
                int maintenance = query2.record().value(0).toInt();
                int stationPlaceId = query2.record().value(1).toInt();
                int stationId = query2.record().value(2).toInt();
                qDebug() << "Maintenance:" << maintenance << ",Station place ID:" << stationPlaceId << ",Station ID:" << stationId;
                QSqlQuery query3(m_database->db());
                if (maintenance == 1)
                {
                    //Wartungsfall
                    //Stationsplatz inaktiv setzen
                    query3.prepare("UPDATE vpj.station_place SET state_id = 3 WHERE station_place_id = :station_place_id");
                    query3.bindValue(":station_place_id", stationPlaceId);
                    query3.exec();
                }
                else
                {
                    //Stationsplatz freigeben
                    query3.prepare("UPDATE vpj.station_place SET state_id = 0 WHERE station_place_id = :station_place_id");
                    query3.bindValue(":station_place_id", stationPlaceId);
                    query3.exec();
                }
                //Station vorbereiten für Freigabe
                query3.prepare("UPDATE vpj.station SET clearing_time = NOW(), state_id = 1 WHERE station_id = :station_id");
                query3.bindValue(":station_id", stationId);
                query3.exec();
                //Werkstücktabelle aktualisieren, ausgecheckt
                query3.prepare("UPDATE vpj.workpiece SET checked_in = 0, station_place_id = ( SELECT destination_station_place_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id ) WHERE workpiece_id = :workpiece_id");
                query3.bindValue(":workpiece_id", workpieceId);
                query3.exec();
            }
        }
        else
        {
            //Einchecken
            QSqlQuery query2(m_database->db());
            //Werkstücktabelle aktualisieren, eingecheckt
            query2.prepare("UPDATE vpj.workpiece SET checked_in = 1 WHERE workpiece_id = (SELECT workpiece_id FROM vpj.workpiece WHERE robot_id = :robot_id)");
            query2.bindValue("robot_id", robotId);
            query2.exec();
        }
    }
}

void Robot::transportFinished(int robotId)
{
    //Station vorbereiten für Freigabe
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.station SET state_id = 1 WHERE station_id = ( SELECT sp.station_id FROM vpj.robot r INNER JOIN vpj.station_place sp ON r.station_place_id = sp.station_place_id WHERE r.robot_id = :robot_id)");
    query.bindValue(":robot_id", robotId);
    query.exec();

    //Ziel: Stationsplatz auf belegt setzten
    query.prepare("UPDATE vpj.station_place SET state_id = 1 WHERE station_place_id = ( SELECT station_place_id FROM vpj.workpiece WHERE robot_id = :robot_id)");
    query.bindValue(":robot_id", robotId);
    query.exec();

    //Werkstück in Bearbeitung setzen
    query.prepare("UPDATE vpj.workpiece SET timestamp = NOW(), workpiece_state_id = 2, robot_id = NULL WHERE workpiece_id = (SELECT workpiece_id FROM vpj.workpiece WHERE robot_id = :robot_id)");
    query.bindValue(":robot_id", robotId);
    query.exec();
}
