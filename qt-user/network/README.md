# network — 用户端Socket封装

本目录封装Qt用户端与PC服务端之间的TCP连接。

## SocketClient职责

1. 连接或断开指定服务端。
2. 自动生成requestId并发送标准RequestMessage。
3. 使用JsonLineCodec处理响应半包和粘包。
4. 检查响应公共字段。
5. 通过Qt信号把响应或错误交给页面。

## 信号说明

| 信号 | 页面处理建议 |
| --- | --- |
| connected | 更新连接状态，允许发送请求 |
| disconnected | 提示断线并提供重连 |
| responseReceived | 按requestId找到原页面请求 |
| protocolError | 提示服务端响应格式异常 |
| socketError | 提示连接失败、拒绝或中断 |

SocketClient不理解登录、订单或充值业务；这些内容由页面逻辑和公共Model处理。
