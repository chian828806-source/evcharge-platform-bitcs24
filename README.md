# EVCharge 新能源汽车智能充电服务与运营平台

本项目是计科小学期一周开发项目，目标是实现一套覆盖用户充电、运营管理、数据可视化、机器学习预测和设备模拟接入的综合软件系统。

当前阶段为：

```text
需求与规范冻结阶段
```

本仓库以 `docs/00-SRS-V1.0.md` 作为第一版需求基线。后续数据库、API、任务拆分、测试和答辩均应优先对齐 SRS。

## 1. 项目目标

系统最终应形成真实业务数据闭环：

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

项目评价重点不是页面数量，而是各模块能否通过统一数据和接口形成完整、稳定、可演示的软件系统。

## 2. 系统模块

### 2.1 Qt 用户端

面向新能源汽车车主，覆盖：

- 手机号登录与自动注册
- 用户资料查看与维护
- 头像上传
- 钱包充值
- 地址定位与附近充电站查询
- 腾讯地图导航
- 充电站与充电桩详情查看
- 空闲充电桩选择
- 预约/充电订单创建
- 充电过程模拟
- 停止充电、计费与钱包结算
- 未完成订单检查
- 基于 ML 预测的低拥堵站点推荐

### 2.2 Qt PC 管理端

面向运营管理人员，覆盖：

- 管理员登录
- 今日、本月、总营收统计
- 近 7 日 / 30 日营收趋势
- 充电桩状态统计
- 充电桩管理
- 充电站管理
- 新增充电站并自动生成模拟电桩
- 用户管理
- 用户冻结 / 解冻
- 手机号模糊查询
- 电桩远程重启模拟
- 负荷预测与运营预警查看

### 2.3 后端服务

后端负责统一业务逻辑、数据校验、事务处理、数据库访问封装、Qt Socket 业务服务、REST API 输出、认证授权和错误处理。

### 2.4 数据库

数据库保存平台核心业务数据，包括用户、管理员、充电站、充电桩、充电订单、充值记录、预测结果、设备日志和操作日志。

业务主数据库建议采用 MySQL 8.x。任务书中出现的 QSQLite 是否为硬性要求仍需确认；若必须体现，可作为 Qt 本地缓存或配置存储，不作为主业务数据库。

### 2.5 Web 数据可视化大屏

Web 大屏使用 ECharts 展示运营数据与预测结果，包括实时充电订单数、今日充电量、今日营收、充电桩利用率、状态分布、站点排行、趋势图和预测结果。

### 2.6 Python 机器学习模块

机器学习模块负责历史数据预处理、未来 1h / 6h / 24h 站点负荷预测、空闲电桩数量预测、高峰时段预测、用户端站点推荐、管理端负荷预警和预测结果持久化。

V1 数据来源采用预置模拟历史数据 + 运行时真实订单追加数据。

### 2.7 设备接入与物联网控制

设备模块用于模拟充电桩接入，建议作为 P1 加分模块正式纳入 V1 范围。

它负责 Device Simulator、Device Gateway、TCP Socket 通信、设备上线、心跳、状态上报、充电数据上报、远程重启、ACK 响应和离线判定。

设备协议详见 `docs/07-DEVICE-PROTOCOL.md`。

## 3. 总体架构

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

说明：

- Linux Qt 用户端和 Qt 管理端采用 TCP Socket 与服务端通信。
- Web 大屏和 Python ML 模块采用 REST API 与后端通信。
- 设备接入采用 Serial / TCP Socket。
- Qt 用户端、Qt 管理端和 Web 大屏不得直接修改业务数据库。
- ML 预测结果必须重新进入系统，不得只停留在本地文件或离线图表。

## 4. 当前技术栈

### Qt 客户端

- C++
- Qt
- Qt Widgets
- Qt Network
- Qt WebEngine
- Qt Charts
- QTcpSocket
- QThread / QtConcurrent
- SocketClient / NetworkClient
- 腾讯地图 Web API

### 后端

- Java
- Spring Boot
- Maven
- MyBatis-Plus
- RESTful API
- SpringDoc OpenAPI / Swagger
- 轻量 JWT 认证

具体 JDK、Spring Boot 及依赖版本将在后端基础工程创建时统一确定并冻结。

### 数据库

- MySQL 8.x
- 可选 SQLite：仅作为 Qt 本地缓存或配置存储，是否需要由老师确认

### Web 大屏

- HTML
- CSS
- JavaScript
- ECharts

### 机器学习

- Python
- NumPy
- pandas
- scikit-learn
- XGBoost
- matplotlib

### 设备通信

- TCP Socket
- 可选 Serial
- JSON Lines 消息格式

## 5. 仓库结构

```text
evcharge-platform/

├── backend/
├── qt-user/
├── qt-admin/
├── web-dashboard/
├── ml/
├── device/
│   ├── gateway/
│   └── simulator/
├── database/
│   ├── schema.sql
│   ├── init_data.sql
│   └── README.md
├── docs/
│   ├── 00-SRS-V1.0.md
│   ├── 01-ARCHITECTURE.md
│   ├── 02-DEVELOPMENT-GUIDE.md
│   ├── 03-API.md
│   ├── 04-DATABASE.md
│   ├── 05-GIT-WORKFLOW.md
│   ├── 06-AGENT-GUIDE.md
│   └── 07-DEVICE-PROTOCOL.md
├── README.md
└── .gitignore
```

## 6. 需求与规范文档

| 文档 | 作用 |
| --- | --- |
| `docs/00-SRS-V1.0.md` | 第一版需求基线 |
| `docs/01-ARCHITECTURE.md` | 系统架构、模块边界、通信模型 |
| `docs/02-DEVELOPMENT-GUIDE.md` | 通用开发规范、线程、日志、配置、测试 |
| `docs/03-API.md` | Qt Socket 业务消息、REST API、认证、错误码、变更规则 |
| `docs/04-DATABASE.md` | 数据库规范、表命名、事务、迁移、种子数据 |
| `docs/05-GIT-WORKFLOW.md` | Git 分支、PR、Review、集成节奏 |
| `docs/06-AGENT-GUIDE.md` | Agent 协作输入、边界和验收 |
| `docs/07-DEVICE-PROTOCOL.md` | 设备模拟接入与 TCP Socket 协议 |

## 7. 开发原则

1. SRS 优先，所有设计和编码必须能追踪到需求编号。
2. 优先完成用户充电、运营管理、数据库、API 和 ML 闭环。
3. 设备模块作为高价值 P1，但不得拖累 P0。
4. 每天短周期迭代，尽早联调。
5. 每天结束时尽量保证 `develop` 分支可运行。
6. Socket 业务消息、REST API、数据库结构和公共状态是公共契约，未经评审不得修改。
7. Agent 可以参与编码和调试，但必须遵循最小修改原则。

## 8. Git 工作流

长期分支：

```text
main
develop
```

开发分支：

```text
feature/*
```

Bug 修复：

```text
fix/*
```

提交格式：

```text
<type>: <description>
```

原则上禁止直接在 `main` 分支开发业务功能。

## 9. Definition of Done

一个功能不能以“代码写完了”作为完成标准。

Done 至少要求：

- 需求编号明确。
- 代码完成。
- 本地能够运行。
- 相关 API 联通。
- 数据读写正确。
- 基本异常处理完成。
- 必要测试通过。
- 文档同步更新。
- 已提交 Git。
- 已合并或准备合并 `develop`。

## 10. 当前待确认事项

- 任务书中的 QSQLite 是否为硬性验收要求。
- Qt 双客户端 Socket 业务通信的服务端实现方式。
- 设备模块是否必须展示独立设备模拟通信。
- 预约是否需要复杂时间预约、排队和超时规则。
- 天气数据是否必须调用真实天气 API。
- 大屏 KPI 是否需要按老师偏好微调。
- Linux 虚拟机最终软件版本。
- GitHub 仓库地址和 SSH 登录信息。

在这些问题确认前，禁止提前锁死数据库字段和 API 契约。
