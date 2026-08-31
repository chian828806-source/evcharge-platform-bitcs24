# 系统架构规范

本文档以 `docs/00-SRS-V1.0.md` 为上位需求基线，定义 EVCharge 的模块边界、运行架构、通信模型和关键工程约束。

## 1. 架构原则

1. 优先保证 SRS 中的 MUST 需求和主业务闭环。
2. 采用前后端分离、模块化、单仓库管理。
3. Qt 用户端、Qt 管理端和 Web 大屏不得直接修改主业务数据库。
4. Linux Qt 用户端和 Qt 管理端使用 TCP Socket 完成核心业务通信。
5. Web 大屏和 Python ML 模块使用 REST API 与后端交互。
6. 设备接入层使用 TCP Socket / Serial 与模拟充电桩通信。
7. API、Socket 应用层消息协议、数据库结构、公共状态枚举、设备协议均属于公共契约。
8. 不引入与 5 人 1 周项目目标无关的重型架构。

## 2. 系统上下文

系统内部模块：

- Qt User Client
- Qt Admin Client
- Spring Boot Backend
- MySQL Database
- Web Dashboard
- Python ML Module
- Device Gateway
- Device Simulator

系统外部依赖：

- 腾讯地图 Web API
- 可选天气数据源
- GitHub
- Linux 虚拟机

## 3. 运行架构

```text
Qt User ──────┐
              ├── TCP Socket ──┐
Qt Admin ─────┘                │
                               │
Web Dashboard ── REST ─────────┼── Spring Boot ── MySQL
                               │
Python ML ───── REST ──────────┘
                               │
                        Device Gateway
                               │
                         Serial / TCP
                               │
                      Simulated Charging Pile
```

## 4. 主业务闭环

```text
Qt 用户端产生充电行为
        ↓
Spring Boot 执行业务规则
        ↓
MySQL 保存订单、用户、站点和设备数据
        ↓
Qt 管理端读取运营变化
        ↓
Web 大屏展示统计指标
        ↓
Python ML 使用历史数据预测
        ↓
预测结果写回系统
        ↓
用户端推荐低拥堵站点，管理端展示负荷预警
```

## 5. 模块职责

### 5.1 Qt 用户端

负责车主侧充电流程：手机号登录、资料维护、钱包充值、定位、站点查询、地图导航、电桩选择、订单创建、充电模拟、计费结算、订单查看和预测推荐展示。

Qt 用户端通过统一 `SocketClient` / `NetworkClient` 与服务端通信，不直接访问 MySQL。

### 5.2 Qt 管理端

负责运营管理：管理员登录、营收统计、趋势图表、充电桩状态统计、电桩管理、电站管理、用户管理、冻结/解冻、手机号模糊查询、远程重启和负荷预警展示。

Qt 管理端通过统一 `SocketClient` / `NetworkClient` 与服务端通信并操作业务数据。

### 5.3 Spring Boot 后端

负责认证与授权、Qt TCP Socket 业务服务、REST API、参数校验、业务规则、事务处理、数据访问、错误处理、文件上传、地图 API 代理、设备命令转发或设备状态接入。

分层结构：

```text
Controller
    ↓
Service
    ↓
Mapper
    ↓
MySQL
```

### 5.4 MySQL 数据库

作为主业务数据库，保存用户、管理员、站点、电桩、订单、充值流水、预测结果、设备日志和操作日志。

任务书中的 QSQLite 是否为硬性要求仍需确认。若必须体现，SQLite 只作为 Qt 本地缓存或配置存储，不作为主业务数据库。

### 5.5 Web 大屏

通过 HTTP API 获取数据，使用 ECharts 展示运营指标、状态分布、趋势排行和 ML 预测结果。

### 5.6 Python ML 模块

采用预置模拟历史数据 + 运行时真实订单追加数据进行训练和预测，输出未来 1h / 6h / 24h 负荷、空闲桩数量和高峰时段，并写回 `prediction` 数据。

V1 不强制部署在线推理服务。

### 5.7 设备接入模块

设备模块包含 Device Gateway 和 Device Simulator。

V1 优先使用 TCP Socket，以 JSON Lines 传输设备消息，支持 `HELLO`、`HEARTBEAT`、`STATUS`、`TELEMETRY`、`START`、`STOP`、`RESTART`、`ACK`。

详细协议见 `docs/07-DEVICE-PROTOCOL.md`。

## 6. 多线程模型

系统应体现合理多线程能力：

- Qt UI 线程不得执行阻塞网络请求或长任务。
- Qt 网络请求使用异步机制或 worker。
- 管理端远程重启不阻塞 UI。
- Device Gateway 使用独立接收、心跳监控和命令派发逻辑。
- ML 任务作为独立 Python 进程或脚本运行。

## 7. 外部服务

### 腾讯地图

用于地址转经纬度和路线导航。

推荐由后端代理腾讯地图 Key，避免 Key 散落到客户端。

### 天气数据

V1 默认使用模拟天气字段或预置历史数据；真实天气 API 是可选项。

## 8. 不采用的架构

V1 不引入：

- Spring Cloud
- 微服务网关
- Kafka / RabbitMQ
- Redis Cluster
- Elasticsearch
- Kubernetes
- 完整 OCPP
- 复杂 RBAC
- GraphQL

除非 SRS 变更，否则不得由个人或 Agent 自行引入。
