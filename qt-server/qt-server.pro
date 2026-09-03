# 功能：构建独立运行的 TCP + WebSocket 业务服务端。
QT += core network websockets sql
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = evcharge-qt-server

# REPO_ROOT 供公共协议和各模块 .pri 定位仓库源码。
REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT/qt-server

SOURCES += $$PWD/main.cpp

include($$REPO_ROOT/shared/protocol/protocol.pri)
include($$PWD/models/models.pri)
include($$PWD/network/network.pri)
include($$PWD/database/database.pri)
include($$PWD/common/common.pri)
include($$PWD/repositories/repositories.pri)
include($$PWD/services/user/user-services.pri)
include($$PWD/services/admin/admin-services.pri)
SOURCES += $$PWD/repositories/predictionrepository.cpp \
           $$PWD/services/prediction/predictionservice.cpp \
           $$PWD/handlers/prediction/predictionhandler.cpp \
           $$PWD/handlers/prediction/registerpredictionhandlers.cpp
HEADERS += $$PWD/repositories/predictionrepository.h \
           $$PWD/services/prediction/predictionservice.h \
           $$PWD/handlers/prediction/predictionhandler.h \
           $$PWD/handlers/prediction/registerpredictionhandlers.h
include($$PWD/handlers/user/user-handlers.pri)
include($$PWD/handlers/admin/admin-handlers.pri)
