# 功能：把可复用的地图导航页面构建为静态库，供后续用户端主窗口链接。
QT += core gui widgets webenginewidgets
CONFIG += staticlib c++17
TEMPLATE = lib
TARGET = evcharge-user-map

REPO_ROOT = $$clean_path($$PWD/..)
INCLUDEPATH += $$REPO_ROOT

HEADERS += $$PWD/map/mapnavigationpage.h
SOURCES += $$PWD/map/mapnavigationpage.cpp
