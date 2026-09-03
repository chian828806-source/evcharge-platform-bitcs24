# 功能：构建不依赖界面和数据库的通信层控制台测试。
QT += core network sql
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = network-protocol-tests

# 测试源码同样含中文注释，MSVC必须以UTF-8读取。
msvc: QMAKE_CXXFLAGS += /utf-8

# 测试直接编译被测源码，便于在业务模块出现前独立验证网络基础设施。
REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/qt-server

SOURCES += \
    $$PWD/tst_networkprotocol.cpp \
    $$REPO_ROOT/qt-user/network/socketclient.cpp \
    $$REPO_ROOT/qt-admin/network/adminsocketclient.cpp \
    $$REPO_ROOT/qt-server/database/databasemanager.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registeruserhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registerstationhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registerorderhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/orderhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/user/stationhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/user/userhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/prediction/predictionhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/prediction/registerpredictionhandlers.cpp \
    $$REPO_ROOT/qt-server/network/clientsession.cpp \
    $$REPO_ROOT/qt-server/network/messagedispatcher.cpp \
    $$REPO_ROOT/qt-server/network/sessionmanager.cpp \
    $$REPO_ROOT/qt-server/network/socketserver.cpp \
    $$REPO_ROOT/qt-server/repositories/userrepository.cpp \
    $$REPO_ROOT/qt-server/repositories/stationrepository.cpp \
    $$REPO_ROOT/qt-server/repositories/orderrepository.cpp \
    $$REPO_ROOT/qt-server/repositories/predictionrepository.cpp \
    $$REPO_ROOT/qt-server/services/user/orderservice.cpp \
    $$REPO_ROOT/qt-server/services/user/stationservice.cpp \
    $$REPO_ROOT/qt-server/services/user/userservice.cpp \
    $$REPO_ROOT/qt-server/services/prediction/predictionservice.cpp

HEADERS += \
    $$REPO_ROOT/qt-user/network/socketclient.h \
    $$REPO_ROOT/qt-admin/network/adminsocketclient.h \
    $$REPO_ROOT/qt-server/common/serviceresult.h \
    $$REPO_ROOT/qt-server/database/databasemanager.h \
    $$REPO_ROOT/qt-server/handlers/user/registeruserhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/registerstationhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/registerorderhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/orderhandler.h \
    $$REPO_ROOT/qt-server/handlers/user/stationhandler.h \
    $$REPO_ROOT/qt-server/handlers/user/userhandler.h \
    $$REPO_ROOT/qt-server/handlers/prediction/predictionhandler.h \
    $$REPO_ROOT/qt-server/handlers/prediction/registerpredictionhandlers.h \
    $$REPO_ROOT/qt-server/network/clientsession.h \
    $$REPO_ROOT/qt-server/network/messagedispatcher.h \
    $$REPO_ROOT/qt-server/network/sessionmanager.h \
    $$REPO_ROOT/qt-server/network/socketserver.h \
    $$REPO_ROOT/qt-server/models/userprofile.h \
    $$REPO_ROOT/qt-server/models/stationinfo.h \
    $$REPO_ROOT/qt-server/models/chargingorder.h \
    $$REPO_ROOT/qt-server/models/predictioninfo.h \
    $$REPO_ROOT/qt-server/models/rechargeinfo.h \
    $$REPO_ROOT/qt-server/repositories/orderrepository.h \
    $$REPO_ROOT/qt-server/repositories/predictionrepository.h \
    $$REPO_ROOT/qt-server/repositories/stationrepository.h \
    $$REPO_ROOT/qt-server/repositories/userrepository.h \
    $$REPO_ROOT/qt-server/services/user/stationservice.h \
    $$REPO_ROOT/qt-server/services/user/orderservice.h \
    $$REPO_ROOT/qt-server/services/user/userservice.h \
    $$REPO_ROOT/qt-server/services/prediction/predictionservice.h

include($$REPO_ROOT/shared/protocol/protocol.pri)
