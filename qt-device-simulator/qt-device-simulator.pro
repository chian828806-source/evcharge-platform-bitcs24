QT += core gui widgets network
CONFIG += c++17
TEMPLATE = app
TARGET = evcharge-device-simulator
REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT
SOURCES += $$PWD/main.cpp $$PWD/devicesimulatorwindow.cpp
HEADERS += $$PWD/devicesimulatorwindow.h
include($$REPO_ROOT/shared/protocol/protocol.pri)
