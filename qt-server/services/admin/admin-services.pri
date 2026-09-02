ADMIN_SERVICE_DIR = $$REPO_ROOT/qt-server/services/admin
HEADERS += $$ADMIN_SERVICE_DIR/adminauthservice.h \
           $$ADMIN_SERVICE_DIR/adminanalyticsservice.h \
           $$ADMIN_SERVICE_DIR/adminmanagementservice.h

SOURCES += $$ADMIN_SERVICE_DIR/adminauthservice.cpp \
           $$ADMIN_SERVICE_DIR/adminanalyticsservice.cpp \
           $$ADMIN_SERVICE_DIR/adminmanagementservice.cpp
