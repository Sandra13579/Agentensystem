#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QObject>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QObject *parent = nullptr);
    void Connect();  //Methode zur Herstellung der Datenbankverbindung
    void Disconnect();  //Methode zur Trennung der Datenbankverbindung
    void Exec(QSqlQuery *query);  //Methode zum Lesen und Schreiben auf eine Datenbank

signals:

private:
    QSqlDatabase db;    //repräsentiert die tatsächliche Datenbankverbindung
};

#endif // DATABASE_H
