ADMIN_SERVICE_ROOT = $$REPO_ROOT/qt-server/services
HEADERS += $$ADMIN_SERVICE_ROOT/passwordhasher.h
SOURCES += $$ADMIN_SERVICE_ROOT/passwordhasher.cpp

include($$ADMIN_SERVICE_ROOT/admin/admin-services.pri)
