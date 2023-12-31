#include "robot.h"

Robot::Robot(QObject *parent)
    : QObject{parent}
{
    m_database = new Database("Robot");
    m_database->connect();

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
    this->transport();
    this->charging();
    this->maintenance();
}

Robot::~Robot()
{
    m_database->disconnect();
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
            m_database->updateRobotState(robotId, State::Reserved);
            qDebug() << "Roboter" << robotId << "fährt zur Wartung";
        }

        //Status = inaktiv (Gewerk 2) & status = reserviert (DB) -> Roboter ist am Wartungsplatz angekommen
        if (m_robotStates[robotId] == State::Inactive && robotState == State::Reserved)
        {
            m_database->updateRobotState(robotId, State::Inactive, m_robotPlaces[robotId]);
            qDebug() << "Roboter" << robotId << "befindet sich in Wartung";
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

        if (m_robotStates[robotId] == State::Fault && robotState != State::Fault && robotState != State::Available)
        {
            m_database->updateRobotState(robotId, State::Fault);
        }

        //Roboterstatus DB = frei & Roboterstatus Gewerk 2 = reserviert -> Roboter hat Auftrag erhalten
        if (robotState == State::Available && m_robotStates[robotId] == State::Reserved)
        {
            //Roboter reservieren
            qDebug() << "Transport: Robot" << robotId << "is reserved";
            m_database->updateRobotState(robotId, State::Reserved);
        }

        //Roboterstatus DB = reserviert & Roboterstatus Gewerk2 = belegt -> Roboter transportiert Werkstück
        if (robotState == State::Reserved && m_robotStates[robotId] == State::Assigned)
        {
            //Roboter hat Werkstück -> belegt
            qDebug() << "Transport: Robot" << robotId << "is assigned";
            m_database->updateRobotState(robotId, State::Assigned, m_robotPlaces[robotId]);
        }

        //Roboterstatus DB = belegt & Roboterstatus Gewerk2 = bereit zum Lesen -> Roboter steht mit Werkstück am RFID Reader
        if (robotState == State::Assigned && m_robotStates[robotId] == State::ReadyForReading)
        {
            qDebug() << "Transport: Robot" << robotId << "is ready for reading";
            m_database->updateRobotState(robotId, State::ReadyForReading, m_robotPlaces[robotId]);
            reading(robotId);
        }

        //Roboterstatus DB = bereit zum Lesen & Roboterstatus Gewerk2 = belegt -> Roboter transportiert Werkstück
        if (robotState == State::ReadyForReading && m_robotStates[robotId] == State::Assigned)
        {
            qDebug() << "Transport: Robot" << robotId << "is assigned";
            checking(robotId);
            m_database->updateRobotState(robotId, State::Assigned, m_robotPlaces[robotId]);
        }

        //Roboterstatus DB = „belegt“ & Roboterstatus (Gewerk2) = „frei“, dann rufe auf „Transportauftragabgeschlossenfunktion“
        if (robotState == State::Assigned && m_robotStates[robotId] == State::Available)
        {
            qDebug() << "Transport: Robot" << robotId << "is available";
            transportFinished(robotId);
            m_database->updateRobotState(robotId, State::Available);
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

        //Roboterstatus DB = frei & Roboterstatus Gewerk 2 = reserviert -> Roboter hat Auftrag erhalten
        if (robotState == State::Available && m_robotStates[robotId] == State::Reserved)
        {
            //Roboter reservieren
            qDebug() << "Charging: Robot" << robotId << "is reserved";
            m_chargingStationStates[placeId] = State::Initial;
            m_database->updateRobotState(robotId, State::Reserved);
        }

        //wenn Roboterstatus „bereit zum Laden“ (Gewerk2) & Roboterstaus "reserviert" (DB)
        if (m_robotStates[robotId] == State::ReadyForCharging && robotState == State::Reserved)
        {
            qDebug() << "Charging: Roboter" << robotId << " bereit zum Laden";
            robotState = State::ReadyForCharging;
            m_database->updateRobotState(robotId, State::ReadyForCharging, m_robotPlaces[robotId]);
        }

        //wenn Roboterstatus „bereit zum Laden“ (Gewerk2) & (Roboterstatus (Gewerk4) =„initial") & Roboterstatus = "kein Fehler" (DB)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Initial && robotState != State::Fault)
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

                    //qDebug() << "Timestamp DB:" << clearingTime;
                    //qDebug() << "Timestamp Programm:" << QDateTime::currentDateTime();
                    if (clearingTime.secsTo(QDateTime::currentDateTime()) > maxDelay)
                    {
                        qDebug() << "Charging: Fehler beim Verbinden der Ladestation" << placeId << "mit dem Roboter" << robotId;
                        m_database->updateRobotState(robotId, State::Fault);
                    }
                }
            }
        }

        //roboterstatus = bereit zum Laden (DB) und Ladestation = verbunden (Gewerk 4)
        if (robotState == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Connected)
        {
            m_database->updateRobotState(robotId, State::Connected);
            qDebug() << "Charging: Roboter" << robotId << " ist mit Ladestation" << placeId << "verbunden";
            QSqlQuery query(m_database->db());
            //query.prepare("UPDATE vpj.station_place SET state_id = 1 WHERE station_place_id = (SELECT station_place_id FROM vpj.robot WHERE robot_id = :robot_id) AND state_id <> 1");
            query.prepare("UPDATE vpj.station_place SET state_id = 1 WHERE station_id = 9 AND place_id = :place_id AND state_id <> 1");
            query.bindValue(":place_id", placeId);
            query.exec();
            emit charge(true, placeId, robotId);
            qDebug() << "Charging: Ladevorgang wird gestartet";
        }

        //jetzt wird geladen status = laden (Gewerk4) und status = bereit zum laden (Gewerk2)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Charging)
        {
            if (robotState != State::Charging)
            {
                m_database->updateRobotState(robotId, State::Charging);
                qDebug() << "Charging: Roboter" << robotId << " lädt";
            }
        }

        //fertig geladen status = frei (Gewerk4) und status = bereit zum laden (Gewerk2)
        if (m_robotStates[robotId] == State::ReadyForCharging && m_chargingStationStates[placeId] == State::Available)
        {
            if (robotState != State::Available)
            {
                m_database->updateRobotState(robotId, State::Available);
                m_chargingStationStates[placeId] = State::Initial; //<- statt publish s.o.
                qDebug() << "Charging: Ladevorgang für Roboter" << robotId << "an Ladestation" << placeId << "beendet";
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
        qDebug() << "Reading: RFID reader at station" << stationId << "on";
        emit rfidOn(stationId); //Turn on the RFID reader at the station
    }
}

void Robot::continueReading(int stationId, int serialNumber)
{
    if (serialNumber != 0)
    {
        qDebug() << "Reading: Continue reading sequence at station" << stationId << "with serial number" << serialNumber;
        emit rfidOff(stationId);
        QSqlQuery query(m_database->db());
        query.prepare("SELECT rfid, workpiece_id FROM vpj.workpiece INNER JOIN vpj.station_place ON workpiece.station_place_id = station_place.station_place_id WHERE station_place.station_id = :station_id AND workpiece.workpiece_state_id = 3");
        query.bindValue(":station_id", stationId);
        query.exec();
        if (query.next())
        {
            int ratedSerialNumber = query.record().value(0).toInt();
            int workpiece_id = query.record().value(1).toInt();
            if (serialNumber != ratedSerialNumber)
            {
                qDebug() << "Reading Error: Read serial number" << serialNumber << "is not equal to rated serial number" << ratedSerialNumber << "in database!";
                //Write fault state into database
                QSqlQuery query2(m_database->db());
                query2.prepare("UPDATE vpj.station SET state_id = 4 WHERE station_id = :station_id");
                query2.bindValue(":station_id", stationId);
                query2.exec();
                return;
            }
            qDebug() << "Reading: Rated serial number:" << ratedSerialNumber << "is ok";
            QSqlQuery query2(m_database->db());
            query2.prepare("SELECT robot_id from vpj.workpiece WHERE workpiece_id = :workpiece_id");
            query2.bindValue(":workpiece_id", workpiece_id);
            query2.exec();
            //Determine robot number (from DB) and initiate "checkout".

            if (query2.next())
            {
                int robotNo = query2.record().value(0).toInt();
                qDebug() << "Reading: Robot" << robotNo << "transport workpiece with number" << serialNumber;
                emit check(robotNo);
            }
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
        //qDebug() << "Checked in:" << checkedIn << "Workpiece ID:" << workpieceId;

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

                if (maintenance == 1)
                {
                    //Wartungsfall: Stationsplatz inaktiv setzen
                    m_database->updateStationPlaceState(stationPlaceId, State::Inactive);
                }
                else
                {
                    //Stationsplatz freigeben
                    m_database->updateStationPlaceState(stationPlaceId, State::Available);
                }
                m_database->updateStationPlaceHistory(stationPlaceId);
                //Station vorbereiten für Freigabe
                QSqlQuery query3(m_database->db());
                query3.prepare("UPDATE vpj.station SET clearing_time = NOW(), state_id = 1 WHERE station_id = :station_id");
                query3.bindValue(":station_id", stationId);
                query3.exec();

                //Werkstücktabelle aktualisieren, ausgecheckt
                query3.prepare("UPDATE vpj.workpiece SET checked_in = 0, station_place_id = ( SELECT destination_station_place_id FROM vpj.workpiece WHERE workpiece_id = :workpiece_id ) WHERE workpiece_id = :workpiece_id");
                query3.bindValue(":workpiece_id", workpieceId);
                query3.exec();
                //m_database->updateWorkpieceHistory(workpieceId);
            }
        }
        else
        {
            //Einchecken
            QSqlQuery query2(m_database->db());
            //Werkstücktabelle aktualisieren, eingecheckt
            query2.prepare("UPDATE vpj.workpiece SET checked_in = 1 WHERE workpiece_id = :workpiece_id");
            query2.bindValue(":workpiece_id", workpieceId);
            query2.exec();
            //m_database->updateWorkpieceHistory(workpieceId);
        }
    }
}

void Robot::transportFinished(int robotId)
{
    qDebug() << "Transport finished!";
    //Station vorbereiten für Freigabe
    QSqlQuery query(m_database->db());
    query.prepare("UPDATE vpj.station SET state_id = 1 WHERE station_id = ( SELECT sp.station_id FROM vpj.robot r INNER JOIN vpj.station_place sp ON r.station_place_id = sp.station_place_id WHERE r.robot_id = :robot_id)");
    query.bindValue(":robot_id", robotId);
    query.exec();

    //Ziel: Stationsplatz auf belegt setzten
    query.prepare("SELECT station_place_id FROM vpj.workpiece WHERE robot_id = :robot_id");
    query.bindValue(":robot_id", robotId);
    query.exec();
    query.next();
    int stationPlaceId = query.record().value(0).toInt();
    m_database->updateStationPlaceState(stationPlaceId, State::Assigned);
    m_database->updateStationPlaceHistory(stationPlaceId);

    //Werkstück in Bearbeitung setzen
    query.prepare("SELECT workpiece_id FROM vpj.workpiece WHERE robot_id = :robot_id");
    query.bindValue(":robot_id", robotId);
    query.exec();
    query.next();
    int workpieceId = query.record().value(0).toInt();
    query.prepare("UPDATE vpj.workpiece SET timestamp = NOW(), workpiece_state_id = 2, robot_id = NULL WHERE workpiece_id = :workpiece_id");
    query.bindValue(":workpiece_id", workpieceId);
    query.exec();
    m_database->updateWorkpieceHistory(workpieceId);
}
