# network — 管理员端 Socket 封装

本目录向 Qt 管理界面提供统一的 TCP 通信入口。管理员页面发送登录、统计、站点、电桩和用户管理请求时，只调用 `AdminSocketClient`。

| 文件 | 功能 |
| --- | --- |
| `adminsocketclient.h/.cpp` | 连接业务服务端、发送标准请求、缓冲半包、解析标准响应 |

管理员页面不得直接创建 `QTcpSocket`、拼接 JSON 字符串或访问 SQLite。消息名称、公共字段、错误码和分帧方式统一引用 `shared/protocol/`。

典型调用顺序：

```text
管理页面输入
  -> AdminSocketClient::sendRequest
  -> TCP JSON Lines
  -> qt-server/network
  -> Handler / Service / Repository
  -> AdminSocketClient::responseReceived
  -> 管理页面更新
```
