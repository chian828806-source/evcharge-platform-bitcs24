# 系统架构规范

本文档以 `docs/00-SRS-V1.0.md` 为上位需求基线。

## 1. 架构原则

1. Qt/C++、SQLite、Socket 和多线程是项目主线。
2. Qt 用户端作为业务客户端，只通过 Socket 与 PC 服务端通信。
3. Qt/C++ PC 服务与管理端同时承担 Socket 服务端、业务处理、数据库访问和管理界面职责。
4. SQLite 是主业务数据库，通过 QtSql 的 `QSQLITE` 驱动访问。
5. Web 大屏通过 WebSocket 与 Qt/C++ PC 服务与管理端交换展示数据。
6. Spring Boot、MySQL 和 REST 不作为 V1 主架构。

## 2. 系统上下文

系统内部模块：

- Qt User Client
- Qt/C++ Server Admin
- SQLite Database
- Web Dashboard
- Python ML Module
- Remote Restart Simulation

系统外部依赖：

- 腾讯地图 Web API
- VMware 17
- Ubuntu 22.04+
- Qt Creator 6.2+

## 3. 运行架构

```text
Qt 用户端（QTcpSocket）
          │
          ▼
Qt/C++ PC 服务与管理端
（QTcpServer + 管理界面 + QChart + 多线程）
          │
          ▼
QtSql + SQLite（QSQLITE）
          │
          ├── 腾讯地图 Web API / QWebEngineView
          ├── Web + ECharts 大屏（WebSocket）
          └── Python 机器学习预测模块
```

## 4. 模块职责

### 4.1 Qt 用户端

负责车主侧业务界面和交互。用户端通过统一 `SocketClient` / `NetworkClient` 发送业务消息，不直接访问 SQLite。

### 4.2 Qt/C++ PC 服务与管理端

负责：

- `QTcpServer` 连接管理；
- Socket 消息解析和响应；
- 用户、站点、电桩、订单、充值、结算业务；
- QtSql / SQLite 数据访问；
- 管理员登录；
- 营收统计和 QChart 图表；
- 电站、电桩和用户管理；
- 远程重启模拟；
- WebSocket 大屏数据服务；
- ML 数据导入导出；
- 多线程任务调度。

### 4.3 SQLite 数据库

保存业务主数据。数据库文件由 PC 服务端统一读写。多线程访问时，每个数据库线程必须使用独立 Qt 数据库连接。

### 4.4 Web 大屏

使用 ECharts 展示运营统计和预测数据。V1 通过 WebSocket 连接 PC 服务与管理端，接收运营概览、状态分布、趋势和预测结果。

### 4.5 Python ML 模块

读取固定演示数据、SQLite 导出数据或 CSV/JSON，输出负荷、空闲桩和高峰时段预测结果。

### 4.6 远程重启模拟

必做范围是管理端发起重启、服务端处理指令、更新状态和记录日志。完整设备协议为扩展内容。

## 5. 多线程模型

PC 服务与管理端至少应划分：

- UI Thread：管理界面和图表展示；
- Socket Accept / Read Thread：处理连接和消息读取；
- Business Worker：执行业务逻辑；
- Charging Timer Worker：维护充电计时和电量计算；
- Database Worker：协调 SQLite 写操作；
- Chart Refresh Worker：定时刷新统计数据。

如老师要求 pthread，应在后续实现中补充 pthread 示例；否则优先使用 Qt 原生 `QThread`。

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

qt-server-admin/network
  ├── SocketServer
  ├── ClientSession
  ├── SessionManager
  ├── MessageDispatcher
  └── DashboardWebSocketServer
~~~

shared/protocol是公共代码，不是业务Service。它不能访问UI、Service或SQLite。

### 5.2 通信线程边界

- Socket线程负责连接、字节收发、分帧和消息投递；
- Business Worker负责业务规则；
- Database Worker或Repository所属线程负责SQLite；
- WebSocket服务负责订阅关系和推送，不自行统计数据；
- UI线程只响应信号并更新界面；
- 跨线程通过Qt信号槽或线程安全队列传递普通数据；
- QTcpSocket、QWebSocket和QSqlDatabase不得跨线程直接使用。

具体实现规范和联调验收见docs/03-API.md第16至24节。

## 6. 不采用的主架构

V1 不采用：

- Spring Boot 主后端；
- MySQL 主数据库；
- REST 主业务接口；
- 微服务；
- 完整 OCPP；
- Kafka / RabbitMQ；
- Kubernetes。
