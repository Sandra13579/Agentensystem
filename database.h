#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QObject>
#include <QDebug>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QString connectionName);
    void Connect();  //Methode zur Herstellung der Datenbankverbindung
    void Disconnect();  //Methode zur Trennung der Datenbankverbindung
    QSqlDatabase db() const { return m_db; } //Übergabe an query!

private:
    QSqlDatabase m_db;    //repräsentiert die tatsächliche Datenbankverbindung

};

#endif // DATABASE_H
