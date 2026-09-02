# 功能：把Qt用户端业务Service源码加入qmake工程。
HEADERS += \
    $$REPO_ROOT/qt-server/services/user/orderservice.h \
    $$REPO_ROOT/qt-server/services/user/stationservice.h \
    $$REPO_ROOT/qt-server/services/user/userservice.h

SOURCES += \
    $$REPO_ROOT/qt-server/services/user/orderservice.cpp \
    $$REPO_ROOT/qt-server/services/user/stationservice.cpp \
    $$REPO_ROOT/qt-server/services/user/userservice.cpp
