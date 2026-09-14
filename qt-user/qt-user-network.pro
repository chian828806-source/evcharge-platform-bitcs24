# 功能：把SocketClient和公共协议构建为Qt用户端可链接的静态库。
QT += core network
QT -= gui

# 静态库不产生独立程序入口，后续Qt Widgets用户端直接链接它。
CONFIG += staticlib c++17
TEMPLATE = lib
TARGET = evcharge-user-network

REPO_ROOT = $$clean_path($$PWD/..)

HEADERS += $$PWD/network/socketclient.h
SOURCES += $$PWD/network/socketclient.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
