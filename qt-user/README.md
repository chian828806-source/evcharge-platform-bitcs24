# qt-user — Qt 用户端

本目录承载车主侧 Qt 界面。当前分支只提供可供所有页面复用的 SocketClient 静态库。

用户端是业务客户端，只连接 `qt-server`，不连接 `qt-admin`，不直接访问 SQLite。

## 文件说明

| 路径 | 功能 |
| --- | --- |
| network/ | TCP连接、请求发送和响应接收 |
| qt-user-network.pro | qmake静态库工程 |

## 页面调用原则

页面只调用 `SocketClient::sendRequest`，不直接操作 `QTcpSocket`。页面保存 `sendRequest` 返回的 `requestId`，并在 `responseReceived` 信号中按 `requestId` 匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。
