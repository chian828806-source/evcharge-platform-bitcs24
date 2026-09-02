# 功能：把管理员客户端Socket封装和公共协议构建为可链接的静态库。
QT += core network
QT -= gui

# 管理界面工程链接此库，不在页面中直接创建QTcpSocket。
CONFIG += staticlib c++17
TEMPLATE = lib
TARGET = evcharge-admin-network

REPO_ROOT = $$clean_path($$PWD/..)

HEADERS += $$PWD/network/adminsocketclient.h
SOURCES += $$PWD/network/adminsocketclient.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
