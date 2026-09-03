QT += core gui widgets network
CONFIG += c++17 utf8_source
TEMPLATE = app
TARGET = evcharge-user

REPO_ROOT = $$clean_path($$PWD/..)

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/ui/userwindow.cpp \
    $$PWD/network/socketclient.cpp

HEADERS += \
    $$PWD/ui/userwindow.h \
    $$PWD/network/socketclient.h

RESOURCES += $$PWD/resources/user-resources.qrc

include($$REPO_ROOT/shared/protocol/protocol.pri)
