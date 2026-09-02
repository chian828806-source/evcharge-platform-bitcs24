# 功能：把服务端共用Repository源码加入qmake工程。
HEADERS += \
    $$REPO_ROOT/qt-server/repositories/orderrepository.h \
    $$REPO_ROOT/qt-server/repositories/stationrepository.h \
    $$REPO_ROOT/qt-server/repositories/userrepository.h

SOURCES += \
    $$REPO_ROOT/qt-server/repositories/orderrepository.cpp \
    $$REPO_ROOT/qt-server/repositories/stationrepository.cpp \
    $$REPO_ROOT/qt-server/repositories/userrepository.cpp
