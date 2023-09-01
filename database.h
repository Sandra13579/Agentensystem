#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QObject>

class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database(QString connectionName);
    void Connect();  //Methode zur Herstellung der Datenbankverbindung
    void Disconnect();  //Methode zur Trennung der Datenbankverbindung
    void Exec(QSqlQuery *query);  //Methode zum Lesen und Schreiben auf eine Datenbank
    QSqlDatabase db() const { return _db; } //Übergabe an query!

private:
    QSqlDatabase _db;    //repräsentiert die tatsächliche Datenbankverbindung

};

#endif // DATABASE_H
