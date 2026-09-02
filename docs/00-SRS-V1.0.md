# 软件需求规格说明书 SRS V1.0

项目：东软电动汽车充电桩应用管理平台

状态：V0.95 / V1.0 Freeze Candidate

上位来源：《东软电动汽车充电桩应用管理平台 项目要求书 Ver1.0》、团队评审结论

## 1. 引言

### 1.1 目的

本文档定义第一版需求基线候选，用于后续架构、数据库、Socket 消息、任务拆分、测试验收和答辩准备。

### 1.2 项目范围

系统包括：

- Linux + Qt 用户端；
- Linux + Qt/C++ 服务端；
- Linux + Qt/C++ 管理端；
- QtSql + SQLite 主数据库，驱动名为 `QSQLITE`；
- WebSocket + ECharts 大数据可视化大屏；
- Python 机器学习预测模块；
- 远程重启模拟功能。

Spring Boot、MySQL 和 REST 不再作为 V1 主架构。

### 1.3 需求优先级

| 优先级 | 含义 |
| --- | --- |
| MUST | 任务书明确提出，或不实现则核心闭环无法成立 |
| SHOULD | 重要增强或由需求推导出的协作功能 |
| OPTIONAL | 加分功能，不得影响 MUST |
| NFR | 非功能需求 |
| OI | 待确认问题 |

## 2. 系统概述

### 2.1 产品定位

本系统面向新能源汽车充电场景，重点展示 Qt/C++、Socket、SQLite、多线程、Qt 图表、Web 大屏和机器学习预测的综合应用。

### 2.2 用户角色

| 角色 | 描述 |
| --- | --- |
| User | 车主，通过 Qt 用户端完成充电服务 |
| Admin | 管理员，通过 Qt 管理端完成运营管理 |
| Dashboard Viewer | 大屏查看者，通过 Web 大屏查看统计和预测 |
| ML Job | 机器学习任务，只处理服务端导出的训练数据并输出预测结果文件 |

### 2.3 主业务闭环

```text
Qt 用户端
   ↓ Socket
Qt/C++ 服务端
   ↓ QtSql
SQLite
   ↓ 服务端导出 CSV/JSON
Python ML
   ↓ 预测 JSON
Qt/C++ 服务端校验并导入 SQLite
   ↓ Socket / WebSocket
Qt 用户端推荐 / Qt 管理端 QChart / Web ECharts
```

### 2.4 通信模型

系统以 Socket 为主通信模型。

Qt 用户端通过 `QTcpSocket` 连接 Qt/C++ 服务端。Qt 管理端也通过 `QTcpSocket` 连接 Qt/C++ 服务端。服务端使用 `QTcpServer` 接收连接，并通过统一应用层消息协议完成用户、充电站、订单、充值、结算及管理业务的数据交互。

Web 大屏 V1 采用 WebSocket 连接 Qt/C++ 服务端。服务端通过 WebSocket 推送或按请求返回运营概览、充电桩状态、营收趋势和预测结果。本文档不使用 REST 作为主接口方案。

Python ML 模块不直接连接、读取或写入 SQLite。Qt/C++ 服务端负责导出经过字段约束的 CSV/JSON 训练数据；ML 模块只读取该导出数据并生成预测 JSON；服务端校验并以事务导入预测结果。FastAPI 不属于 V1 数据链路，也不能替代 Qt/C++ 主服务。

## 3. 功能需求

### 3.1 Qt 用户端

#### FR-U-001 手机号登录与自动注册

Priority: MUST

用户输入 11 位手机号。手机号存在则登录；手机号不存在则自动注册，默认昵称为 `用户` + 手机号后四位，初始余额为 0。

验收：

- 合法新手机号首次登录后生成用户记录。
- 同一手机号重复登录不会重复注册。
- 冻结用户默认不能进入新业务；如存在活动订单，仅允许进入订单停止与结算相关的收尾流程。

#### FR-U-002 用户信息查看

Priority: MUST

用户端显示头像、昵称、手机号和钱包余额。

#### FR-U-003 修改昵称

Priority: MUST

昵称不能为空，长度为 2 到 20 个字符。

#### FR-U-004 修改头像

Priority: MUST

用户端从本地选择头像图片，通过 Socket 消息传给 Qt/C++ 服务端。服务端保存头像文件，数据库保存相对路径。

#### FR-U-005 钱包充值

Priority: MUST

用户输入充值金额，系统模拟支付成功，余额增加，并记录充值流水。

### 3.2 充电站查询与导航

#### FR-U-006 地址定位

Priority: MUST

用户通过下拉选择区域或手动输入地址模拟当前位置。

#### FR-U-007 地址转经纬度

Priority: MUST

Qt/C++ 服务端或用户端使用腾讯地图 Web API 将地址转换为经纬度。地图 Key 不写入公开仓库。

#### FR-U-008 附近充电站排序

Priority: MUST

系统根据当前位置和站点经纬度按距离升序展示充电站。

计算口径：

- 使用 Haversine 公式或统一的平面近似算法；
- 距离单位为 km，保留 2 位小数；
- 排序按计算距离从小到大。

#### FR-U-009 充电站卡片

Priority: MUST

每个站点至少显示站名、价格、电桩总数、空闲数量和距离。

空闲数量口径：`AVAILABLE` 状态电桩数量。

#### FR-U-010 查看站内充电桩

Priority: MUST

展示电桩编号、类型、状态和功率。

#### FR-U-011 地图导航

Priority: MUST

用户端使用 `QWebEngineView` 加载腾讯地图导航页面，至少支持驾车和步行模式。

### 3.3 充电业务

#### FR-C-001 未完成订单检测

Priority: MUST

用户创建新订单前，服务端检查是否存在活动订单。

```text
activeOrderStatuses = {CREATED, CHARGING, PENDING_PAYMENT}
```

#### FR-C-002 选择空闲充电桩

Priority: MUST

只有 `AVAILABLE` 状态电桩可被选择。

#### FR-C-003 创建预约或充电订单

Priority: MUST

V1 将“预约”简化为选择空闲桩后创建 `CREATED` 状态订单，并将对应电桩置为 `RESERVED`，避免被其他用户再次选择。

#### FR-C-004 开始充电

Priority: MUST

服务端统一启动充电计时，订单进入 `CHARGING`，电桩由 `RESERVED` 进入 `CHARGING`。

#### FR-C-005 充电过程展示

Priority: MUST

用户端展示由服务端计算或推送的充电时长、电量和预估金额。

#### FR-C-006 停止充电

Priority: MUST

用户手动停止充电，服务端记录结束时间、充电时长和充电量，订单进入 `PENDING_PAYMENT`。

#### FR-C-007 计费与钱包结算

Priority: MUST

计费公式：

```text
amountFen = round(energyKwh × (priceFenPerKwh + serviceFeeFenPerKwh))
```

金额以“分”为整数保存和计算。余额不足时订单保持 `PENDING_PAYMENT`，重复结算不得重复扣款。

#### FR-C-008 订单查看

Priority: MUST

用户可以查看历史订单和当前未完成订单。

#### FR-C-009 取消未开始订单

Priority: MUST

用户可以取消尚未开始充电的订单。仅允许 `CREATED` 状态订单取消；取消成功后订单进入 `CANCELLED`，对应电桩由 `RESERVED` 恢复为 `AVAILABLE`。

### 3.4 Qt 管理端

#### FR-A-001 管理员登录

Priority: MUST

默认账号为 `admin / 123456`。数据库不得保存明文密码，应使用可靠哈希方式保存。

#### FR-A-002 核心营收指标

Priority: MUST

展示今日营收、本月营收和总营收。营收只统计 `COMPLETED` 订单。

#### FR-A-003 营收趋势

Priority: MUST

使用 QChart 展示近 7 日和近 30 日营收趋势。

日期口径：

- 近 7 日包含当天；
- 近 30 日包含当天；
- 日期按系统本地日期计算。

#### FR-A-004 电桩状态统计

Priority: MUST

展示在用、闲置、故障数量和占比。

统计口径：

- 闲置：`AVAILABLE`；
- 在用：`CHARGING`；
- 故障：`FAULT`；
- 离线和重启中可单独展示，不并入闲置。

#### FR-A-005 充电桩列表

Priority: MUST

展示 `pileNo`、所属站点、类型、功率、状态、累计充电次数和累计充电时长。

#### FR-A-006 远程重启电桩

Priority: MUST

管理员发起远程重启模拟，系统记录指令、返回处理结果，并将电桩短暂置为 `RESTARTING` 后恢复为 `AVAILABLE` 或原可用状态。

#### FR-A-007 充电站列表

Priority: MUST

展示站点编号、名称、地址、经纬度、电桩数量和在线率。

#### FR-A-008 查看站内电桩实时状态

Priority: MUST

管理员可查看指定站点下电桩状态。

#### FR-A-009 新增充电站

Priority: MUST

管理员新增充电站，并输入站名、地址、经纬度和电桩数量。V1 自动生成对应数量的模拟电桩。

#### FR-A-010 用户列表

Priority: MUST

展示用户编号、手机号、昵称、余额、注册时间和状态。

#### FR-A-011 冻结用户

Priority: MUST

管理员可将用户状态从 `NORMAL` 修改为 `FROZEN`。冻结后用户不能创建新业务；如没有活动订单，可直接拒绝登录或进入只读提示页。

若被冻结用户已经存在活动订单，系统仍允许该用户完成停止充电和结算，不允许继续创建预约、开始新的充电业务或充值。

#### FR-A-012 解冻用户

Priority: MUST

管理员可将用户状态从 `FROZEN` 修改为 `NORMAL`。

#### FR-A-013 手机号模糊查询

Priority: MUST

管理员可按手机号关键字查询用户。

### 3.5 Web 数据可视化大屏

#### FR-D-001 运营概览

Priority: MUST

MUST 指标：

- 今日充电量；
- 今日营收；
- 充电桩状态分布；
- 近 7 日充电趋势；
- 站点负荷预测。

数据来源为 Qt/C++ 服务端提供的 WebSocket 消息，以及固定演示数据和预测结果。

#### FR-D-002 状态与趋势图表

Priority: MUST

使用 ECharts 展示状态分布、近 7 日趋势和站点负荷预测。

#### FR-D-003 预测结果展示

Priority: MUST

大屏至少展示站点负荷预测；空闲桩预测和高峰时段预测在数据存在时展示。

### 3.6 机器学习

#### FR-ML-001 历史数据预处理

Priority: MUST

使用固定演示历史数据和运行时订单数据，输入字段至少包括站点、时间、充电时长、充电量、价格、是否节假日和天气字段占位。

运行时订单数据必须由 Qt/C++ 服务端导出为训练数据文件，ML 模块不得直接查询 SQLite。

#### FR-ML-002 负荷预测

Priority: MUST

生成 1h / 6h / 24h 站点负荷预测。

站点充电负荷统一定义为指定时间窗口内的电桩占用率：

```text
stationLoad = chargingPileMinutes / (totalPileCount × windowMinutes)
```

其中 `chargingPileMinutes` 只统计处于 `CHARGING` 状态的实际充电占用时长，不统计 `RESERVED`、`FAULT`、`OFFLINE` 或 `RESTARTING`。结果取值范围为 0 到 1，展示时可转换为百分比。ML、大屏和用户推荐必须使用该统一口径。

验收：

- 每个目标站点至少生成三个时间窗口预测；
- 输出包含 `stationId`、`horizon`、`predictionTime`、`predictedLoad`；
- 可用 MAE / RMSE 做基本评价，但不把高精度作为硬性验收。

#### FR-ML-003 空闲桩数量预测

Priority: MUST

输出 `predictedAvailableCount`，预测值为非负数。

#### FR-ML-004 高峰时段预测

Priority: MUST

输出高峰时段或高峰等级。

#### FR-ML-005 用户侧站点推荐

Priority: MUST

用户端可展示基于预测负荷或空闲桩数量的推荐站点。

#### FR-ML-006 管理端负荷预警

Priority: MUST

管理端可展示站点级负荷预警。

#### FR-ML-007 服务端数据交换与结果导入

Priority: MUST

Qt/C++ 服务端负责导出 ML 训练数据，并导入 ML 生成的预测 JSON。导入前必须校验数据格式、站点存在性、预测时间窗和预测取值范围；一个批次校验失败时不得部分写入。ML 模块不得持有 SQLite 文件路径、连接信息或写库权限。

### 3.7 Qt/C++ 服务端支撑

#### FR-S-001 Socket 服务

Priority: MUST

服务端使用 `QTcpServer` 接收 Qt 用户端和 Qt 管理端连接，统一完成消息分帧、鉴权、路由、错误码返回和会话管理。

#### FR-S-002 业务服务

Priority: MUST

服务端集中实现用户、站点、电桩、订单、充值、结算、管理统计和远程重启等业务规则。Qt 用户端和 Qt 管理端不得绕过服务端直接修改 SQLite。

#### FR-S-003 数据库访问

Priority: MUST

服务端是唯一通过 QtSql 的 `QSQLITE` 驱动读写 SQLite 的模块，并保证订单、余额、电桩状态、操作日志和 ML 预测结果导入的事务一致。所有客户端、Web 大屏和 Python ML 均不得直接访问 SQLite。

#### FR-S-004 WebSocket 大屏服务

Priority: MUST

服务端向 Web 大屏提供 WebSocket 数据服务，统一推送运营概览、状态分布、趋势和预测结果。

### 3.8 设备与远程控制

#### FR-IOT-001 远程重启模拟

Priority: MUST

必做范围只包括远程重启指令、返回结果、状态变化和操作日志。

#### FR-IOT-002 完整设备协议

Priority: OPTIONAL

完整设备网关、设备心跳、遥测、串口通信和 ACK 协议为扩展内容，不代替 Qt 用户端、Qt 管理端与 Qt/C++ 服务端之间的 Socket 主通信。

## 4. 业务规则

### BR-001 用户唯一性

手机号唯一。

### BR-002 冻结用户限制

冻结用户不能充值、创建订单或开始新的充电业务，历史数据保留。若冻结前已有活动订单，仍允许执行停止充电和结算。

### BR-003 活动订单限制

每个用户同一时刻最多存在 1 个活动订单，活动状态为：

```text
{CREATED, CHARGING, PENDING_PAYMENT}
```

### BR-004 单桩互斥

每个电桩同一时刻最多服务 1 个订单。订单为 `CREATED` 时电桩状态为 `RESERVED`，订单为 `CHARGING` 时电桩状态为 `CHARGING`。

### BR-005 订单状态转换

```text
CREATED -> CHARGING -> PENDING_PAYMENT -> COMPLETED
CREATED -> CANCELLED
PENDING_PAYMENT -> PENDING_PAYMENT（余额不足）
```

普通用户不能直接取消 `CHARGING` 订单。

电桩正常状态流转：

```text
AVAILABLE -> RESERVED -> CHARGING -> AVAILABLE
AVAILABLE -> RESERVED -> AVAILABLE（订单取消）
```

### BR-006 统计口径

- 空闲桩数量只统计 `AVAILABLE`；
- 站点充电负荷使用 `stationLoad = chargingPileMinutes / (totalPileCount × windowMinutes)`；
- 今日营收只统计当天 `COMPLETED` 订单；
- 本月营收只统计本月 `COMPLETED` 订单；
- 7/30 日趋势包含当天；
- 订单金额以分为单位保存和计算。

## 5. 状态模型

用户状态：

```text
NORMAL
FROZEN
```

电桩状态：

```text
AVAILABLE
RESERVED
CHARGING
FAULT
OFFLINE
RESTARTING
```

订单状态：

```text
CREATED
CHARGING
PENDING_PAYMENT
COMPLETED
CANCELLED
```

## 6. 数据需求

V1 至少包含：

- User
- Admin
- ChargingStation
- ChargingPile
- ChargingOrder
- RechargeRecord
- Prediction
- OperationLog

数据库表名和字段名使用 snake_case。Qt/C++ 类名使用 PascalCase，变量和函数使用 camelCase。

## 7. 外部接口需求

### 7.1 Qt Socket 业务通信

Qt 用户端、Qt 管理端与 Qt/C++ 服务端采用 TCP Socket，消息格式详见 `docs/03-API.md`。

### 7.2 Web 大屏数据

V1 通过 WebSocket 获取大屏数据，消息格式详见 `docs/03-API.md`。

### 7.3 腾讯地图

使用腾讯地图 Web API 和 `QWebEngineView` 完成定位转换和导航展示。

### 7.4 ML 集成

服务端导出 CSV/JSON 训练数据给 ML；ML 输出预测 JSON；服务端校验后导入 SQLite。ML 不直接访问 SQLite。

## 8. 非功能需求

### NFR-ENV-001 运行环境

固定环境：

- VMware 17；
- Ubuntu 22.04+；
- Qt Creator 6.2+；
- C++；
- Qt Widgets / Network / Sql / Charts / WebEngine。
- Qt WebSockets。

### NFR-THR-001 多线程

主程序应为多线程结构。Qt/C++ 服务端至少区分：

- Socket 连接处理；
- 业务处理；
- 充电计时任务；
- 数据库写入协调；
- WebSocket 大屏推送。

Qt 用户端和 Qt 管理端不得在 UI 线程中执行阻塞式网络等待或长时间任务。

是否必须直接使用 pthread，仍需向老师确认；若无强制要求，V1 使用 `QThread`。

### NFR-ERR-001 错误处理

系统必须处理断线、消息不完整、数据库失败、地图失败、余额不足、重复结算和非法状态转换。

## 9. 安全需求

### SEC-001 管理员密码

管理员密码不得明文保存，应使用可靠哈希方式。

### SEC-002 敏感配置

地图 Key、数据库路径、管理员初始密码等敏感信息不得硬编码进公开仓库。

### SEC-003 会话认证

Qt Socket 登录成功后返回随机会话编号。后续受保护消息携带 `sessionId`。管理员和普通用户会话必须区分权限。

## 10. 验收主流程

### 10.1 用户充电

1. 手机号登录。
2. 定位并查询附近站点。
3. 查看站点和电桩。
4. 选择空闲桩创建订单。
5. 开始充电并查看过程。
6. 停止充电。
7. 结算扣款。
8. 查看订单历史。

### 10.2 运营管理

1. 管理员登录。
2. 查看营收和状态图表。
3. 管理站点和电桩。
4. 查询、冻结、解冻用户。
5. 模拟远程重启。

### 10.3 智能运营

1. 准备固定演示数据。
2. 运行 ML 预测。
3. 输出 1h / 6h / 24h 预测结果。
4. 用户端展示推荐。
5. 管理端和大屏展示预测或预警。

## 11. 需求追踪矩阵

| 模块 | 核心需求 | Source | 依赖文档 |
| --- | --- | --- | --- |
| 用户端 | FR-U-001 至 FR-U-011，FR-C-001 至 FR-C-009 | Task Book | `docs/03-API.md`，`docs/04-DATABASE.md` |
| Qt 管理端 | FR-A-001 至 FR-A-013 | Task Book | `docs/03-API.md` |
| Qt/C++ 服务端 | FR-S-001 至 FR-S-004 | Team Definition | `docs/03-API.md`，`docs/04-DATABASE.md` |
| 大屏 | FR-D-001 至 FR-D-003 | Task Book + Team Definition | `docs/03-API.md` |
| ML | FR-ML-001 至 FR-ML-006 | Task Book | `docs/04-DATABASE.md` |
| 远程控制 | FR-IOT-001 至 FR-IOT-002 | Task Book + Optional Enhancement | `docs/07-DEVICE-PROTOCOL.md` |
| 多线程 | NFR-THR-001 | Task Book | `docs/01-ARCHITECTURE.md` |

## 12. 待确认问题

### OI-001 多线程实现形式

需确认是否必须直接使用 pthread，还是 Qt 的 `QThread` 可以满足要求。

### OI-002 ML 数据与精度

需确认老师是否提供统一数据集，以及是否要求最低预测精度。

### OI-003 天气与节假日

若没有真实数据，V1 使用固定演示字段或模拟字段。

## 13. 不纳入 V1 主线

- Spring Boot 主后端；
- MySQL 主数据库；
- REST 主业务接口；
- 微服务；
- Kafka / RabbitMQ；
- Redis Cluster；
- Elasticsearch；
- Kubernetes；
- 完整 OCPP；
- 复杂 RBAC；
- 区块链；
- LLM 客服。
