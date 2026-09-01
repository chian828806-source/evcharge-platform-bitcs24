# 功能：qmake顶层工程，按顺序构建服务端、两个客户端网络库和协议测试。
# 说明：三个应用边界彼此独立，只通过shared/protocol复用公共协议契约。
TEMPLATE = subdirs
CONFIG += ordered

server.file = qt-server/qt-server.pro
user_network.file = qt-user/qt-user-network.pro
admin_network.file = qt-admin/qt-admin-network.pro
network_tests.file = tests/network/network-protocol-tests.pro

SUBDIRS += server user_network admin_network network_tests
