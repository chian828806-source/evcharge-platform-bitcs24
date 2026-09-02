QT += core sql testlib
QT -= gui
CONFIG += console testcase c++17
TEMPLATE = app
TARGET = tst_adminmanagement

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/qt-server $$REPO_ROOT

SOURCES += $$PWD/tst_adminmanagement.cpp \
           $$REPO_ROOT/qt-server/services/admin/adminmanagementservice.cpp \
           $$REPO_ROOT/qt-server/repositories/repositories.cpp \
           $$REPO_ROOT/shared/protocol/protocolmessage.cpp

HEADERS += $$REPO_ROOT/qt-server/services/admin/adminmanagementservice.h \
           $$REPO_ROOT/shared/protocol/protocolmessage.h \
           $$REPO_ROOT/shared/protocol/errorcodes.h
