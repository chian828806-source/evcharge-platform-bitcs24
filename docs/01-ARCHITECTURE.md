# 系统架构规范

本文档以 `docs/00-SRS-V1.0.md` 为上位需求基线。

## 1. 架构原则

1. Qt/C++、SQLite、Socket 和 Qt 事件驱动是项目主线。
2. Qt 用户端作为业务客户端，只通过 Socket 与 Qt/C++ 服务端通信。
3. Qt 管理端作为独立管理客户端，只通过 Socket 与 Qt/C++ 服务端通信。
4. Qt/C++ 服务端承担业务 TCP、独立 Device TCP、业务处理、数据库访问和 Dashboard
   WebSocket 数据服务，不承担管理界面职责。
5. SQLite 是主业务数据库，通过 QtSql 的 `QSQLITE` 驱动访问。
6. Web 大屏通过 WebSocket 与 Qt/C++ 服务端交换展示数据。
7. Spring Boot、MySQL 和 REST 不作为 V1 主架构。

## 2. 系统上下文

系统内部模块：

- Qt User Client
- Qt Admin Client
- Qt/C++ Server
- SQLite Database
- Web Dashboard
- Python ML Module
- Qt Device Simulator

系统外部依赖：

- 腾讯地图 Web API
- VMware 17
- Ubuntu 22.04+
- Qt Creator 6.2+

## 3. 运行架构

```text
Qt 用户端（QTcpSocket） ─┐
                         ├── 业务 TCP :18080 ── SocketServer ─┐
Qt 管理端（QTcpSocket） ─┘                                   │
                                                               ├── MessageDispatcher / SessionManager
Qt Device Simulator ── 设备 TCP :18082 ── DeviceGatewayServer ├── Handler → Service → Repository
                                                               ├── QtSql + SQLite（QSQLITE）
Web + ECharts ── WebSocket :18081/dashboard ──────────────────┤
                                                               ├── 腾讯地图 Web API（异步适配）
                                                               └── Prediction 导入/查询
```

## 4. 模块职责

### 4.1 Qt 用户端

负责车主侧业务界面和交互。用户端通过统一 `SocketClient` / `NetworkClient` 发送用户业务消息，不直接访问 SQLite。

### 4.2 Qt 管理端

负责运营管理界面和图表展示。管理端通过统一 `SocketClient` / `NetworkClient` 发送管理业务消息，不直接访问 SQLite，不启动 `QTcpServer`，不实现业务规则。

管理端负责：

- 管理员登录界面；
- 营收统计、趋势和 QChart 展示；
- 电站、电桩和用户管理界面；
- 冻结/解冻、手机号模糊查询界面；
- 远程重启操作入口；
- 根据服务端返回结果刷新界面和提示错误。

### 4.3 Qt/C++ 服务端

服务端的唯一组合根是 `qt-server/main.cpp`。它创建一份 `DatabaseManager`、
`SessionManager`、`MessageDispatcher`、业务 `SocketServer`、`DeviceRegistry`、
`DeviceControlService`、`DeviceGatewayServer` 与 `DashboardWebSocketServer`。负责：

- `QTcpServer` 连接管理；
- Socket 消息解析和响应；
- 用户、站点、电桩、订单、充值、结算业务；
- 管理端业务请求处理；
- QtSql / SQLite 数据访问；
- 管理员登录；
- 营收、状态和趋势统计；
- 设备会话、心跳超时、遥测缓存、故障、重连和远程重启 ACK；
- WebSocket 大屏数据服务；
- ML 数据导入导出；
- 事件驱动的定时任务与异步回调调度。

### 4.4 SQLite 数据库

保存业务主数据。数据库文件由 Qt/C++ 服务端统一读写。多线程访问时，每个数据库线程必须使用独立 Qt 数据库连接。

### 4.5 Web 大屏

使用 ECharts 展示运营统计和预测数据。V1 通过 WebSocket 连接 Qt/C++ 服务端，接收运营概览、状态分布、趋势和预测结果。

### 4.6 Python ML 模块

只读取固定演示数据或 Qt/C++ 服务端导出的 CSV/JSON，输出负荷、空闲桩和高峰时段预测 JSON。ML 不直接访问 SQLite，结果由服务端校验后导入。

### 4.7 Device Simulator 与设备网关

设备协议是与 User/Admin TCP 会话隔离的 JSON Lines TCP 通道，默认端口 `18082`，
当前用于 Qt Device Simulator，不是 OCPP，也没有生产级 TLS 或设备认证。`DEVICE_HELLO`
完成后服务端才把电桩视为当前进程受管设备；心跳、遥测、故障状态与 `DEVICE_ACK` 由
`DeviceSession`、`DeviceRegistry` 和 `DeviceControlService` 协作处理。

服务端数据库状态是业务权威：受管 `OFFLINE` 电桩在合法 HELLO 且无活动订单时恢复
`AVAILABLE`；`FAULT` 不会因重连自动清除，必须经管理员正式重启流程恢复。详细消息和
状态规则见 `docs/07-DEVICE-PROTOCOL.md`。

## 5. 多线程模型

当前实现以单个 Qt 事件循环、非阻塞 `QTcpSocket`/`QWebSocket` 信号槽和异步地图回调
驱动；它没有把 Socket、数据库和 Dashboard 固化为独立 Worker 线程。耗时计算或外部
调用不得阻塞 `readyRead` 回调。未来若引入 `QThread`，每个线程必须创建自己的
`QSqlDatabase` 连接，且 `QTcpSocket`、`QWebSocket`、`QSqlDatabase` 不能跨线程直接使用。

### 5.1 通信模块结构

通信相关代码按以下方式分层：

~~~text
shared/protocol
  ├── Message Types
  ├── Error Codes
  ├── Request / Response
  └── JsonLineCodec

qt-user/network
  └── SocketClient

qt-admin/network
  └── SocketClient

qt-server/network
  ├── SocketServer
  ├── ClientSession
  ├── SessionManager
  ├── MessageDispatcher
  └── DashboardWebSocketServer

qt-server/devices
  ├── DeviceGatewayServer / DeviceSession
  ├── DeviceRegistry
  └── DeviceControlService
~~~

shared/protocol是公共代码，不是业务Service。它不能访问UI、Service或SQLite。

### 5.2 通信线程边界

- 当前事件循环负责连接、字节收发、分帧、投递和定时心跳检查；
- Handler/Service 负责业务规则，Repository 负责 SQLite 访问；
- WebSocket服务负责订阅关系和推送，不自行统计数据；
- Qt 用户端和 Qt 管理端 UI 线程只响应信号并更新界面；
- 跨线程通过Qt信号槽或线程安全队列传递普通数据；
- QTcpSocket、QWebSocket和QSqlDatabase不得跨线程直接使用。

具体实现规范和联调验收见 `docs/03-API.md` 第 16 至 24 节。

## 6. 不采用的主架构

V1 不采用：

- Spring Boot 主后端；
- MySQL 主数据库；
- REST 主业务接口；
- 微服务；
- 完整 OCPP；
- Kafka / RabbitMQ；
- Kubernetes。
