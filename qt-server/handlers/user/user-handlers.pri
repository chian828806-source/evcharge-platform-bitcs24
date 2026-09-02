# 功能：把用户端Handler源码加入qmake工程。
HEADERS += \
    $$REPO_ROOT/qt-server/handlers/user/orderhandler.h \
    $$REPO_ROOT/qt-server/handlers/user/registerorderhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/registerstationhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/registeruserhandlers.h \
    $$REPO_ROOT/qt-server/handlers/user/registeruserbackend.h \
    $$REPO_ROOT/qt-server/handlers/user/stationhandler.h \
    $$REPO_ROOT/qt-server/handlers/user/userhandler.h

SOURCES += \
    $$REPO_ROOT/qt-server/handlers/user/orderhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registerorderhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registerstationhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registeruserhandlers.cpp \
    $$REPO_ROOT/qt-server/handlers/user/registeruserbackend.cpp \
    $$REPO_ROOT/qt-server/handlers/user/stationhandler.cpp \
    $$REPO_ROOT/qt-server/handlers/user/userhandler.cpp
