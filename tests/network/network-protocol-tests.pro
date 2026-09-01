# 功能：构建不依赖界面和数据库的通信层控制台测试。
QT += core network
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = network-protocol-tests

# 测试直接编译被测源码，便于在业务模块出现前独立验证网络基础设施。
REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/qt-server-admin

SOURCES += \
    $$PWD/tst_networkprotocol.cpp \
    $$REPO_ROOT/qt-server-admin/network/messagedispatcher.cpp \
    $$REPO_ROOT/qt-server-admin/network/sessionmanager.cpp

HEADERS += \
    $$REPO_ROOT/qt-server-admin/network/messagedispatcher.h \
    $$REPO_ROOT/qt-server-admin/network/sessionmanager.h

include($$REPO_ROOT/shared/protocol/protocol.pri)
