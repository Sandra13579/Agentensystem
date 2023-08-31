QT = core
QT += sql      #damit sql benutzt werden kann
QT += mqtt

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    database.cpp \
    main.cpp \
    dispatcher.cpp \
    robot.cpp \
    station.cpp \
    workpiece.cpp \
    interface.cpp

HEADERS += \
    database.h \
    dispatcher.h \
    global.h \
    robot.h \
    station.h \
    workpiece.h \
    interface.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
