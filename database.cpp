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

