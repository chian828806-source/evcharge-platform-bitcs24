# 功能：qmake顶层工程，按顺序构建服务端、用户端网络库和协议测试。
# 说明：各子工程独立引用shared/protocol，避免复制公共协议代码。
TEMPLATE = subdirs
CONFIG += ordered

server.file = qt-server-admin/qt-server-admin.pro
user_network.file = qt-user/qt-user-network.pro
network_tests.file = tests/network/network-protocol-tests.pro

SUBDIRS += server user_network network_tests
