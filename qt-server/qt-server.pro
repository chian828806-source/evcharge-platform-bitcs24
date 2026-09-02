# 功能：构建独立运行的TCP + WebSocket业务服务端。
QT += core network websockets sql
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcharge-qt-server

# REPO_ROOT供公共协议和服务端network.pri定位仓库源码。
REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT/qt-server

SOURCES += $$PWD/main.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
include($$PWD/models/models.pri)
include($$PWD/network/network.pri)
include($$PWD/database/database.pri)
include($$PWD/common/common.pri)
include($$PWD/repositories/repositories.pri)
include($$PWD/services/services.pri)
include($$PWD/services/user/user-services.pri)
include($$PWD/handlers/admin/admin-handlers.pri)
include($$PWD/handlers/user/user-handlers.pri)
