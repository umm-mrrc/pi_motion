#-------------------------------------------------
#
# Project created by QtCreator 2015-03-02T15:50:55
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = robotPanel
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    stepper.cpp

HEADERS  += mainwindow.h \
    stepper.h \
    pi_stepper_pins.h

FORMS    += mainwindow.ui

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../usr/local/lib/release/ -lwiringPi
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../usr/local/lib/debug/ -lwiringPi
else:symbian: LIBS += -lwiringPi
else:unix: LIBS += -L$$PWD/../../../../../usr/local/lib/ -lwiringPi

INCLUDEPATH += $$PWD/../../../../../usr/local/include
DEPENDPATH += $$PWD/../../../../../usr/local/include

QMAKE_CXXFLAGS_DEBUG -= -O2
QMAKE_CXXFLAGS_DEBUG += -O0

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O0
