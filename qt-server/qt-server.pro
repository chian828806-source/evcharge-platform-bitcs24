# 功能：构建独立运行的TCP + WebSocket业务服务端。
QT += core network sql websockets
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcharge-qt-server

# 源码和注释统一按UTF-8保存，避免MSVC使用本地代码页解析中文时破坏预处理。
msvc: QMAKE_CXXFLAGS += /utf-8

# REPO_ROOT供公共协议和服务端network.pri定位仓库源码。
REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT/qt-server

SOURCES += $$PWD/main.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
include($$PWD/models/models.pri)
include($$PWD/network/network.pri)
include($$PWD/database/database.pri)
include($$PWD/repositories/repositories.pri)
include($$PWD/services/user/user-services.pri)
include($$PWD/handlers/user/user-handlers.pri)
