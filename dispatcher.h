#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class dispatcher; }
QT_END_NAMESPACE

class dispatcher : public QMainWindow
{
    Q_OBJECT

public:
    dispatcher(QWidget *parent = nullptr);
    ~dispatcher();

private:
    Ui::dispatcher *ui;
};
#endif // DISPATCHER_H
