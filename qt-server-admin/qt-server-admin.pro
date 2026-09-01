# 功能：构建可运行的TCP + WebSocket服务端网络外壳。
QT += core network websockets
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcharge-network-server

# REPO_ROOT供共享protocol.pri和network.pri定位仓库源码。
REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT/qt-server-admin

SOURCES += $$PWD/main.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
include($$PWD/network/network.pri)
