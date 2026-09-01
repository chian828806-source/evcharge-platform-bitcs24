# qt-user — Qt用户端

本目录最终承载车主侧Qt界面。当前分支只提供可供所有页面复用的SocketClient静态库。

## 文件说明

| 路径 | 功能 |
| --- | --- |
| network/ | TCP连接、请求发送和响应接收 |
| qt-user-network.pro | qmake静态库工程 |

## 页面调用原则

页面只调用 SocketClient::sendRequest，不直接操作QTcpSocket。页面保存sendRequest返回的requestId，并在 responseReceived 信号中按requestId匹配响应。

登录成功后，页面所属的Session对象应保存服务端返回的sessionId；之后的受保护请求都传入该值。
