# 功能：把TCP与WebSocket服务端通信源码加入qt-server工程。
# 说明：业务Service和Repository不应列入本文件。
HEADERS += \
    $$REPO_ROOT/qt-server/network/clientsession.h \
    $$REPO_ROOT/qt-server/network/dashboardwebsocketserver.h \
    $$REPO_ROOT/qt-server/network/messagedispatcher.h \
    $$REPO_ROOT/qt-server/network/sessionmanager.h \
    $$REPO_ROOT/qt-server/network/socketserver.h

SOURCES += \
    $$REPO_ROOT/qt-server/network/clientsession.cpp \
    $$REPO_ROOT/qt-server/network/dashboardwebsocketserver.cpp \
    $$REPO_ROOT/qt-server/network/messagedispatcher.cpp \
    $$REPO_ROOT/qt-server/network/sessionmanager.cpp \
    $$REPO_ROOT/qt-server/network/socketserver.cpp
