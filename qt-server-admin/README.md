# qt-server-admin — PC服务端网络外壳

本目录最终承载Qt/C++ PC服务与管理端。当前分支只实现网络基础设施和一个可运行的控制台入口，尚未实现管理界面及业务Service。

## 目录与文件

| 路径 | 功能 |
| --- | --- |
| main.cpp | 创建Session、Dispatcher、TCP服务和WebSocket服务 |
| network/ | 连接、分帧、鉴权、分发和大屏推送 |
| qt-server-admin.pro | qmake服务端工程 |

## 当前运行结果

- TCP默认监听18080。
- WebSocket默认监听18081，路径为 /dashboard。
- 未注册业务Handler时，合法消息返回5002，不伪造业务结果。

## 后端队友如何接入

业务负责人实现Service后，在程序装配阶段调用 MessageDispatcher::registerHandler：

~~~cpp
dispatcher.registerHandler(
    MessageTypes::OrderCreate,
    MessageDispatcher::Access::User,
    [&orderService](const RequestMessage &request,
                    const SessionContext &session) {
        return orderService.createOrder(
            request.requestId, session.principalId, request.payload);
    });
~~~

Handler负责取参数和调用Service；Service负责业务规则；Repository负责SQL和事务。三层不得混写。

## 构建

在仓库外创建构建目录后执行：

~~~bash
qmake6 /home/bit/workspace/evcharge-platform-bitcs24/qt-server-admin/qt-server-admin.pro
make -j2
./evcharge-network-server
~~~
