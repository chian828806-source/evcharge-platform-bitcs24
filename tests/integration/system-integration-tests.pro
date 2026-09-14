# 功能：以独立进程、临时SQLite和真实TCP Socket验证服务端装配边界。
QT += core network sql
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = system-integration-tests

msvc: QMAKE_CXXFLAGS += /utf-8

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT
DEFINES += EVCHARGE_REPO_ROOT=\\\"$$REPO_ROOT\\\"

SOURCES += $$PWD/tst_systemintegration.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
