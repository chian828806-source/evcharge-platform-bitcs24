# qt-server-admin — 历史合并目录

本目录来自早期“服务端和管理端合并”的方案。当前项目目标架构已经调整为服务端和管理端分离：

- `qt-server/`：Qt/C++ 服务端，负责 Socket 服务、业务规则、SQLite、WebSocket 和 ML 对接；
- `qt-admin/`：Qt 管理端，负责管理界面和 QChart，通过 Socket 调用服务端。

当前分支中本目录只实现网络基础设施和一个可运行的控制台入口，尚未实现管理界面及业务 Service。后续编码时，应将服务端通信骨架迁移到 `qt-server/`，管理端界面新建到 `qt-admin/`。

## 目录与文件

| 路径 | 功能 |
| --- | --- |
| main.cpp | 早期服务端入口，后续迁移到 `qt-server/` |
| network/ | 连接、分帧、鉴权、分发和大屏推送，后续迁移到 `qt-server/network/` |
| qt-server-admin.pro | 早期 qmake 工程，后续拆分为服务端工程和管理端工程 |

## 当前运行结果

- TCP默认监听18080。
- WebSocket默认监听18081，路径为 /dashboard。
- 未注册业务Handler时，合法消息返回5002，不伪造业务结果。

## 服务端队友如何接入

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

该构建命令仅适用于当前过渡目录。完成目录迁移后，应以 `qt-server/qt-server.pro` 为准。
