REPOSITORY_DIR = $$REPO_ROOT/qt-server/repositories

HEADERS += $$REPOSITORY_DIR/repositorybase.h \
           $$REPOSITORY_DIR/adminrepository.h \
           $$REPOSITORY_DIR/userrepository.h \
           $$REPOSITORY_DIR/stationrepository.h \
           $$REPOSITORY_DIR/pilerepository.h \
           $$REPOSITORY_DIR/orderrepository.h \
           $$REPOSITORY_DIR/operationlogrepository.h

SOURCES += $$REPOSITORY_DIR/repositorybase.cpp \
           $$REPOSITORY_DIR/adminrepository.cpp \
           $$REPOSITORY_DIR/userrepository.cpp \
           $$REPOSITORY_DIR/stationrepository.cpp \
           $$REPOSITORY_DIR/pilerepository.cpp \
           $$REPOSITORY_DIR/orderrepository.cpp \
           $$REPOSITORY_DIR/operationlogrepository.cpp
