QT += core gui widgets network charts
CONFIG += c++17
TEMPLATE = app
TARGET = evcharge-qt-admin

REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT

HEADERS += $$PWD/mainwindow.h \
           $$PWD/network/adminsocketclient.h
HEADERS += $$PWD/ui/adminpages.h
SOURCES += $$PWD/main.cpp \
           $$PWD/mainwindow.cpp \
           $$PWD/network/adminsocketclient.cpp
SOURCES += $$PWD/ui/adminpages.cpp

RESOURCES += $$PWD/resources/admin-resources.qrc

include($$REPO_ROOT/shared/protocol/protocol.pri)
