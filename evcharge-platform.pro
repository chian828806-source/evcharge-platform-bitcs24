# 功能：qmake顶层工程，按顺序构建服务端、两个客户端网络库和协议测试。
# 说明：三个应用边界彼此独立，只通过shared/protocol复用公共协议契约。
TEMPLATE = subdirs
CONFIG += ordered

server.file = qt-server/qt-server.pro
user_network.file = qt-user/qt-user-network.pro
user_app.file = qt-user/qt-user.pro
admin.file = qt-admin/qt-admin.pro
network_tests.file = tests/network/network-protocol-tests.pro
admin_tests.file = tests/admin/admin-management-tests.pro

SUBDIRS += server user_network user_app admin network_tests admin_tests
