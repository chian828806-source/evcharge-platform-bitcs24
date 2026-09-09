QT += core network sql testlib
QT -= gui
CONFIG += console testcase c++17
TEMPLATE = app
TARGET = tst_adminmanagement

REPO_ROOT = $$clean_path($$PWD/../..)
INCLUDEPATH += $$REPO_ROOT/qt-server $$REPO_ROOT

SOURCES += $$PWD/tst_adminmanagement.cpp \
           $$REPO_ROOT/qt-server/services/admin/adminmanagementservice.cpp \
           $$REPO_ROOT/qt-server/database/databasemanager.cpp \
           $$REPO_ROOT/qt-server/repositories/repositorybase.cpp \
           $$REPO_ROOT/qt-server/repositories/pilerepository.cpp \
           $$REPO_ROOT/qt-server/repositories/stationrepository.cpp \
           $$REPO_ROOT/qt-server/repositories/userrepository.cpp \
           $$REPO_ROOT/qt-server/repositories/operationlogrepository.cpp \
           $$REPO_ROOT/qt-server/repositories/orderrepository.cpp \
           $$REPO_ROOT/qt-server/devices/deviceregistry.cpp \
           $$REPO_ROOT/qt-server/devices/devicecontrolservice.cpp \
           $$REPO_ROOT/qt-server/devices/devicesession.cpp \
           $$REPO_ROOT/shared/protocol/jsonlinecodec.cpp \
           $$REPO_ROOT/shared/protocol/protocolmessage.cpp

HEADERS += $$REPO_ROOT/qt-server/services/admin/adminmanagementservice.h \
           $$REPO_ROOT/qt-server/devices/deviceregistry.h \
           $$REPO_ROOT/qt-server/devices/devicecontrolservice.h \
           $$REPO_ROOT/qt-server/devices/devicesession.h \
           $$REPO_ROOT/shared/protocol/protocolmessage.h \
           $$REPO_ROOT/shared/protocol/errorcodes.h
