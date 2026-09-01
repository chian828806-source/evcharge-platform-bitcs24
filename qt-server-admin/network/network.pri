# 功能：把服务端通信层源码加入qt-server-admin工程。
# 说明：业务Service和Repository不应列入本文件。
HEADERS += \
    $$REPO_ROOT/qt-server-admin/network/clientsession.h \
    $$REPO_ROOT/qt-server-admin/network/dashboardwebsocketserver.h \
    $$REPO_ROOT/qt-server-admin/network/messagedispatcher.h \
    $$REPO_ROOT/qt-server-admin/network/sessionmanager.h \
    $$REPO_ROOT/qt-server-admin/network/socketserver.h

SOURCES += \
    $$REPO_ROOT/qt-server-admin/network/clientsession.cpp \
    $$REPO_ROOT/qt-server-admin/network/dashboardwebsocketserver.cpp \
    $$REPO_ROOT/qt-server-admin/network/messagedispatcher.cpp \
    $$REPO_ROOT/qt-server-admin/network/sessionmanager.cpp \
    $$REPO_ROOT/qt-server-admin/network/socketserver.cpp
