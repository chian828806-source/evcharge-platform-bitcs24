# 功能：把公共协议源码加入当前qmake工程。
# 前提：引用它的.pro文件必须先设置REPO_ROOT。
INCLUDEPATH += $$REPO_ROOT

HEADERS += \
    $$REPO_ROOT/shared/protocol/errorcodes.h \
    $$REPO_ROOT/shared/protocol/jsonlinecodec.h \
    $$REPO_ROOT/shared/protocol/messagetypes.h \
    $$REPO_ROOT/shared/protocol/protocolmessage.h

SOURCES += \
    $$REPO_ROOT/shared/protocol/jsonlinecodec.cpp \
    $$REPO_ROOT/shared/protocol/messagetypes.cpp \
    $$REPO_ROOT/shared/protocol/protocolmessage.cpp
