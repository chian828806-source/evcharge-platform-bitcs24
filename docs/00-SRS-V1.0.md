# 软件需求规格说明书 SRS V1.0

项目：EVCharge 新能源汽车智能充电服务与运营平台

状态：V0.9 / V1.0 Freeze Candidate

上位来源：《东软电动汽车充电桩应用管理平台 项目要求书 Ver1.0》及团队需求评审建议

## 1. 引言

### 1.1 目的

本文档定义本项目第一版软件需求基线，用于后续架构设计、数据库设计、API 设计、任务拆分、测试验收和答辩准备。

本文档不是对项目任务书的简单摘抄，而是将任务书要求转换为可开发、可测试、可追踪的系统需求。

### 1.2 项目范围

本项目面向新能源汽车充电服务场景，建设一套覆盖用户充电、运营管理、数据可视化、机器学习预测和设备模拟接入的综合软件系统。

系统包括：

- Linux + Qt 充电用户端
- Linux + Qt PC 管理端
- Spring Boot 后端服务
- MySQL 业务数据库
- Web + ECharts 数据可视化大屏
- Python 机器学习智能分析模块
- 设备接入与物联网控制子系统

### 1.3 需求优先级

需求优先级统一使用：

| 优先级 | 含义 |
| --- | --- |
| MUST | 项目要求书明确提出，或不实现则核心业务闭环无法成立 |
| SHOULD | 从正式要求推导出的必要隐含需求，建议纳入 V1.0 |
| OPTIONAL | 加分或优化功能，不得影响 MUST 需求交付 |
| NFR | 非功能需求 |
| CONSTRAINT | 技术、环境、课程或团队约束 |

### 1.4 需求编号

需求编号规则：

```text
FR-U-xxx      用户端
FR-C-xxx      充电业务
FR-A-xxx      管理端
FR-D-xxx      数据大屏
FR-ML-xxx     机器学习
FR-IOT-xxx    设备接入
BR-xxx        业务规则
NFR-xxx       非功能需求
SEC-xxx       安全需求
OI-xxx        待确认问题
```

## 2. 系统概述

### 2.1 产品定位

EVCharge 是面向新能源汽车充电场景的服务与运营平台。系统目标不是分别完成若干独立界面，而是让用户充电行为、后台运营、数据统计、机器学习预测和设备状态形成可演示的数据闭环。

### 2.2 用户角色

| 角色 | 描述 |
| --- | --- |
| User | 新能源汽车车主，通过 Qt 用户端完成充电服务 |
| Admin | 平台运营人员，通过 Qt PC 管理端管理站点、电桩、用户和营收 |
| Operator Viewer | 大屏查看者，通过 Web 大屏查看运营指标和预测结果 |
| ML Job | 机器学习任务，读取历史数据并写回预测结果 |
| Device Simulator | 模拟充电桩，上报状态、心跳、充电数据并接收控制指令 |

### 2.3 系统边界

系统内部：

- Qt 用户端
- Qt 管理端
- Spring Boot 后端
- MySQL 数据库
- Web 数据大屏
- Python ML 模块
- Device Gateway
- Device Simulator

系统外部：

- 腾讯地图 Web API
- 可选天气数据源
- GitHub 仓库
- Linux 虚拟机运行环境

### 2.4 主业务闭环

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

### 2.5 通信模型

系统采用 TCP Socket + REST API 的混合通信模型。

Linux Qt 用户端和 Qt 管理端作为主要业务客户端，采用 TCP Socket 与服务端进行通信，通过统一应用层消息协议完成用户、充电站、订单及管理等核心业务数据交互，以满足项目要求书中 Socket 网络通信的技术要求。

Web 大数据可视化端以及 Python 机器学习模块采用 REST API 与 Spring Boot 后端进行数据交互，以降低跨语言及浏览器端集成复杂度。

如实现充电设备接入子系统，模拟充电桩与设备接入层之间采用 Serial 或 TCP Socket 通信，用于设备心跳、状态上报、充电数据采集及远程控制。

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

## 3. 功能需求

### 3.1 用户端

#### FR-U-001 手机号登录与自动注册

Priority: MUST

Actor: User

Description: 用户输入 11 位手机号登录。手机号存在则直接登录；手机号不存在则自动注册并登录。

Inputs:

- `phone`

Normal Flow:

1. 用户输入手机号。
2. 系统校验手机号格式。
3. 后端查询手机号是否存在。
4. 若存在且状态正常，返回用户信息和认证凭证。
5. 若不存在，创建新用户，默认昵称为 `用户` + 手机号后四位，初始余额为 0。
6. 用户端进入主界面。

Exception Flow:

- 手机号格式非法时，返回明确错误提示。
- 用户状态为 `FROZEN` 时拒绝登录。
- 并发自动注册时不得创建重复用户。

Acceptance Criteria:

- 合法新手机号首次登录后数据库生成一条用户记录。
- 同一手机号重复登录不会重复注册。
- 冻结用户无法进入系统。

#### FR-U-002 用户信息查看

Priority: MUST

Actor: User

Description: 用户端应显示头像、昵称、手机号和钱包余额。

Acceptance Criteria:

- 登录后能查看当前用户资料。
- 钱包充值或结算后余额刷新。

#### FR-U-003 修改昵称

Priority: MUST

Actor: User

Description: 用户可以修改昵称。

Rules:

- 昵称不能为空。
- 昵称长度为 2 到 20 个字符。
- 昵称不得影响其他用户数据。

Acceptance Criteria:

- 合法昵称修改后立即显示新昵称。
- 非法昵称返回明确错误。

#### FR-U-004 修改头像

Priority: MUST

Actor: User

Description: 用户可以从本地选择图片作为头像。

Dependencies:

- 后端应支持头像上传或头像 URL 保存策略。

Recommended Implementation:

- 上传路径：`backend/uploads/avatar/`
- 数据库存储：`avatar_url`

Acceptance Criteria:

- 用户上传头像后再次登录仍可看到头像。
- 非图片文件或超出大小限制时返回错误。

#### FR-U-005 钱包充值

Priority: MUST

Actor: User

Source: Task Book + Team Derived Rule

Description: 用户输入金额并模拟支付成功，系统增加钱包余额并记录充值流水。

Rules:

- 金额必须大于 0。
- 金额使用 `BigDecimal` 和 `DECIMAL(10,2)`。
- 充值操作必须保证余额和流水一致。

Acceptance Criteria:

- 充值成功后余额正确增加。
- 充值记录可追踪。
- 非法金额不会修改余额。

### 3.2 充电站查询与导航

#### FR-U-006 地址定位

Priority: MUST

Actor: User

Description: 用户可以通过下拉选择区域或手动输入地址模拟当前位置。

Acceptance Criteria:

- 系统不依赖真实 GPS 硬件。
- 用户可以获得一个用于附近站点查询的当前位置。

#### FR-U-007 地址转经纬度

Priority: MUST

Actor: User

Description: 系统通过腾讯地图 Web API 将地址转换为经纬度。

Recommended Rule:

- 腾讯地图 Key 优先由后端代理管理，不直接散落在 Qt 客户端。

Acceptance Criteria:

- 合法地址可以获得经纬度。
- 外部地图 API 失败时有明确提示或降级处理。

#### FR-U-008 附近充电站排序

Priority: MUST

Actor: User

Description: 系统根据当前位置按距离由近及远展示充电站。

Data:

- `station.latitude`
- `station.longitude`
- `charger.status`

Acceptance Criteria:

- 返回列表按距离升序排列。
- 每个站点显示距离。

#### FR-U-009 充电站卡片

Priority: MUST

Actor: User

Description: 每个站点卡片至少显示站名、价格、电桩总数、空闲数量和距离。

Acceptance Criteria:

- 空闲数量来自后端统计或数据库状态，不由客户端写死。

#### FR-U-010 查看站内充电桩

Priority: MUST

Actor: User

Description: 用户点击站点后查看站内充电桩列表。

Fields:

- 电桩编号
- 类型
- 状态
- 功率

Acceptance Criteria:

- 电桩状态与管理端一致。
- 不可用电桩不能被选择充电。

#### FR-U-011 地图导航

Priority: MUST

Actor: User

Description: 用户可从当前位置导航到目标充电站，至少支持驾车和步行路线。

Technology:

- 腾讯地图
- `QWebEngineView`

Acceptance Criteria:

- 用户可以打开地图路线页面。
- 至少支持 `DRIVING` 与 `WALKING` 两种模式。

### 3.3 充电业务

#### FR-C-001 未完成订单检测

Priority: MUST

Actor: User

Description: 用户开始新充电前，系统必须检查是否存在未完成订单。

Rules:

- 每个用户同一时刻最多 1 个 active charging order。
- `activeOrderStatuses = {CREATED, CHARGING, PENDING_PAYMENT}`。
- 存在未完成订单时，用户必须先进入结算流程。

Acceptance Criteria:

- 有未完成订单时不能创建新订单。
- 提示用户先完成结算。

#### FR-C-002 选择空闲充电桩

Priority: MUST

Actor: User

Description: 用户只能选择 `AVAILABLE` 状态的电桩进行充电。

Acceptance Criteria:

- `CHARGING`、`FAULT`、`OFFLINE`、`RESTARTING` 状态电桩不可选择。

#### FR-C-003 创建预约或充电订单

Priority: MUST

Actor: User

Description: 用户选择空闲充电桩后创建充电订单。

V1 Simplification:

- “预约”简化为选择空闲桩后创建待启动订单，不做复杂预约时间、锁桩失效和排队规则。

Acceptance Criteria:

- 创建订单后电桩不再被其他用户选择。
- 订单记录用户、电站、电桩、价格和创建时间。
- 订单初始状态为 `CREATED`。

#### FR-C-004 开始充电

Priority: MUST

Actor: User

Description: 用户启动充电，订单进入充电状态，电桩进入占用状态。

Acceptance Criteria:

- 订单状态变为 `CHARGING`。
- 电桩状态变为 `CHARGING`。

#### FR-C-005 充电过程展示

Priority: MUST

Actor: User

Description: 用户端展示模拟充电过程，包括充电时长、电量和预估金额。

Acceptance Criteria:

- 数据随模拟过程变化。
- 页面退出后订单状态不丢失。

#### FR-C-006 停止充电

Priority: MUST

Actor: User

Description: 用户手动停止充电，系统计算充电结果。

Acceptance Criteria:

- 停止后记录结束时间、充电时长和充电量。
- 订单状态从 `CHARGING` 进入 `PENDING_PAYMENT`。
- 电桩状态释放或按设备状态更新。

#### FR-C-007 计费与钱包结算

Priority: MUST

Actor: User

Description: 系统按充电量和电价计算费用，并从用户钱包扣款。

Rules:

- 默认计费公式：`amount = energyKwh × pricePerKwh`。
- 结算必须使用事务。
- 余额不足时不得产生部分扣款。

Acceptance Criteria:

- 订单金额与扣款一致。
- 结算成功后订单状态从 `PENDING_PAYMENT` 进入 `COMPLETED`，余额减少。
- 余额不足时订单保持 `PENDING_PAYMENT` 并提示充值。

#### FR-C-008 订单查看

Priority: MUST

Actor: User

Description: 用户可以查看历史订单和当前未完成订单。

Acceptance Criteria:

- 历史订单包含站点、电桩、时间、电量、金额和状态。

### 3.4 管理端

#### FR-A-001 管理员登录

Priority: MUST

Actor: Admin

Description: 管理员使用用户名和密码登录。

Default Account:

- username: `admin`
- password: `123456`

Security Rule:

- 数据库不得明文保存密码，推荐保存 BCrypt hash。

Acceptance Criteria:

- 正确账号可登录。
- 错误密码被拒绝。

#### FR-A-002 核心营收指标

Priority: MUST

Actor: Admin

Description: 管理端展示今日营收、本月营收和总营收。

Acceptance Criteria:

- 指标基于已完成或已支付订单统计。

#### FR-A-003 营收趋势

Priority: MUST

Actor: Admin

Description: 管理端展示近 7 日和近 30 日营收趋势折线图。

Technology:

- Qt Charts / `QChart`

#### FR-A-004 电桩状态统计

Priority: MUST

Actor: Admin

Description: 管理端展示至少在用、闲置、故障状态的数量和占比。

Acceptance Criteria:

- 状态统计与电桩列表一致。

#### FR-A-005 充电桩列表

Priority: MUST

Actor: Admin

Description: 管理端展示电桩列表。

Fields:

- `pileNo`
- `station`
- `type`
- `power`
- `status`
- `totalChargeCount`
- `totalChargeDuration`

#### FR-A-006 远程重启电桩

Priority: MUST

Actor: Admin

Description: 管理员可对指定电桩发起远程重启模拟。

MUST Scope:

- 管理端发起重启请求。
- 后端记录重启操作。
- 系统模拟重启处理结果并更新状态或日志。

Enhanced By:

- FR-IOT-005 提供真实设备层 `RESTART` 指令和 ACK 链路。

Acceptance Criteria:

- 管理端发起重启后能看到处理结果。
- 重启事件写入日志或设备事件表。
- 即使设备接入模块未完成，也必须提供基础远程重启模拟闭环。

#### FR-A-007 充电站列表

Priority: MUST

Actor: Admin

Description: 管理端展示充电站列表。

Fields:

- `stationId`
- `name`
- `address`
- `longitude`
- `latitude`
- `pileCount`
- `onlineRate`

#### FR-A-008 查看站内电桩实时状态

Priority: MUST

Actor: Admin

Description: 管理员可查看指定站点下电桩状态。

#### FR-A-009 新增充电站

Priority: MUST

Actor: Admin

Description: 管理员新增充电站，并输入站名、地址、经纬度和电桩数量。

V1 Rule:

- 新增站点时自动生成指定数量的模拟电桩。

Acceptance Criteria:

- 新增站点后用户端可查询。
- 自动生成电桩数量与输入数量一致。

#### FR-A-010 用户列表

Priority: MUST

Actor: Admin

Description: 管理端展示用户列表。

Fields:

- `userId`
- `phone`
- `nickname`
- `balance`
- `registerTime`
- `status`

#### FR-A-011 冻结用户

Priority: MUST

Actor: Admin

Description: 管理员可将用户状态从 `NORMAL` 修改为 `FROZEN`。

Acceptance Criteria:

- 冻结后用户无法登录和发起业务操作。
- 历史数据保留。

#### FR-A-012 解冻用户

Priority: MUST

Actor: Admin

Description: 管理员可将用户状态从 `FROZEN` 修改为 `NORMAL`。

#### FR-A-013 手机号模糊查询

Priority: MUST

Actor: Admin

Description: 管理员可按手机号关键字查询用户。

### 3.5 Web 数据可视化大屏

#### FR-D-001 运营概览

Priority: MUST

Actor: Operator Viewer

Description: Web 大屏展示平台运营核心指标。

MUST KPIs:

- 今日充电量
- 今日营收
- 充电桩状态分布
- 近 7 日充电趋势
- 站点负荷预测

SHOULD KPIs:

- 实时充电订单数
- 充电桩利用率
- 在线站点数量

Acceptance Criteria:

- Web 大屏至少展示全部 MUST KPIs。
- MUST KPIs 的数据来自后端 REST API，不使用最终演示用纯静态假数据。
- 当预测结果暂缺时，大屏显示明确降级状态。

#### FR-D-002 状态与排行图表

Priority: MUST

Actor: Operator Viewer

Description: 使用 ECharts 展示状态分布和运营排行。

MUST Charts:

- 充电桩状态分布
- 近 7 日充电趋势
- 站点负荷预测

SHOULD Charts:

- 站点营收排行
- 高峰时段分布

Acceptance Criteria:

- Web 大屏至少包含全部 MUST Charts。
- 图表能根据后端返回数据渲染。
- 图表数据为空时页面不崩溃。

#### FR-D-003 预测结果展示

Priority: MUST

Actor: Operator Viewer

Description: Web 大屏展示站点负荷预测、空闲桩预测和高峰预测结果。

Acceptance Criteria:

- Web 大屏能读取 `prediction` 数据或预测 REST API。
- 至少展示站点负荷预测。
- 空闲桩预测和高峰时段预测在数据存在时可展示。

### 3.6 机器学习

#### FR-ML-001 历史数据预处理

Priority: MUST

Actor: ML Job

Description: ML 模块读取历史充电数据并完成清洗、特征处理和训练集构造。

Data:

- 充电时段
- 充电时长
- 充电量
- 用户出行时段
- 天气
- 节假日

V1 Rule:

- 历史数据采用预置模拟数据 + 运行时真实订单追加数据。

Acceptance Criteria:

- 能读取合法历史数据并完成预处理流程。
- 异常或缺失数据不会导致整个系统崩溃。
- 预处理结果可被训练或预测脚本继续使用。

#### FR-ML-002 负荷预测

Priority: MUST

Actor: ML Job

Description: 预测未来 1 小时、6 小时、24 小时的站点负荷。

Acceptance Criteria:

- 能基于历史数据完成训练或预测流程。
- 每个目标站点至少生成 1h / 6h / 24h 三个时间窗口的预测结果。
- 预测结果能写入 `prediction` 数据。
- 系统可使用 MAE / RMSE 等指标评估模型效果，但 V1 不将高准确率作为硬性验收条件。

#### FR-ML-003 空闲桩数量预测

Priority: MUST

Actor: ML Job

Description: 预测未来指定时间窗口内各站点空闲充电桩数量。

Acceptance Criteria:

- 每个目标站点生成可读取的预测空闲桩数量。
- 预测值应为非负数，并能被用户端推荐或大屏展示读取。

#### FR-ML-004 高峰时段预测

Priority: MUST

Actor: ML Job

Description: 基于历史数据预测高峰充电时段。

Acceptance Criteria:

- 能输出高峰时段或高峰等级。
- 输出结果能写入数据库或通过 REST API 被管理端 / Web 大屏读取。

#### FR-ML-005 用户侧站点推荐

Priority: MUST

Actor: User

Description: 用户端根据预测结果优先推荐低拥堵、高空闲率的站点。

Acceptance Criteria:

- 用户端能读取至少一个站点推荐结果。
- 推荐结果能解释为基于预测负荷或预测空闲桩数量生成。

#### FR-ML-006 管理端负荷预警

Priority: MUST

Actor: Admin

Description: 管理端根据预测结果向运营人员提供负荷预警。

Acceptance Criteria:

- 管理端能展示站点级负荷预警。
- 当预测结果缺失时，系统显示明确降级状态而不是崩溃。

### 3.7 设备接入与物联网控制

#### FR-IOT-001 设备模拟器

Priority: SHOULD

Actor: Device Simulator

Description: 系统提供模拟充电桩，用于模拟设备上线、心跳、状态上报、充电数据上报和控制指令响应。

#### FR-IOT-002 TCP Socket 通信

Priority: SHOULD

Actor: Device Simulator

Description: 设备模拟器和设备接入层通过 TCP Socket 通信，以满足项目要求书中的 Socket 编程要求。

#### FR-IOT-003 设备心跳

Priority: SHOULD

Actor: Device Simulator

Description: 设备周期性发送心跳，后端或网关据此判断设备在线状态。

#### FR-IOT-004 充电数据上报

Priority: SHOULD

Actor: Device Simulator

Description: 设备上报功率、电量、状态和故障信息。

#### FR-IOT-005 远程重启指令

Priority: SHOULD

Actor: Admin

Description: 管理员通过管理端发起重启，后端或网关向设备发送 `RESTART` 指令并接收 ACK。

#### FR-IOT-006 设备状态同步

Priority: SHOULD

Actor: Device Simulator

Description: 设备状态变化应进入数据库，并被管理端和 Web 大屏读取。

## 4. 业务规则

### BR-001 用户唯一性

手机号是用户登录唯一标识，不允许重复。

### BR-002 冻结用户限制

冻结用户不能登录、充值、创建订单或开始充电，但历史数据保留。

### BR-003 单用户活动订单限制

每个用户同一时刻最多存在 1 个活动订单。

活动订单状态定义为：

```text
activeOrderStatuses = {CREATED, CHARGING, PENDING_PAYMENT}
```

`COMPLETED` 和 `CANCELLED` 不属于活动订单。

### BR-004 单桩互斥

每个充电桩同一时刻最多服务 1 个订单。

占用电桩的订单状态至少包括：

```text
pileOccupiedOrderStatuses = {CREATED, CHARGING}
```

订单进入 `PENDING_PAYMENT` 后，电桩可释放为 `AVAILABLE`，或按设备实际状态更新。

### BR-005 订单结算一致性

订单完成、扣减余额、写入结算结果必须保持事务一致性。

### BR-006 计费公式

V1 默认公式：

```text
amount = energyKwh × pricePerKwh
```

### BR-007 新增站点建桩规则

管理端新增站点时，按输入电桩数量自动生成模拟电桩。

### BR-008 ML 结果持久化

ML 预测结果必须写入数据库或通过后端统一接口进入系统，不得只保存在本地临时文件中作为最终成果。

### BR-009 订单状态转换

V1 订单主流程固定为：

```text
CREATED
   ↓ start
CHARGING
   ↓ stop
PENDING_PAYMENT
   ↓ settlement success
COMPLETED
```

余额不足时：

```text
PENDING_PAYMENT → PENDING_PAYMENT
```

取消规则：

```text
CREATED → CANCELLED
```

V1 不允许普通用户将 `CHARGING` 订单直接取消为 `CANCELLED`；如需异常终止，必须由后端按故障流程处理并记录日志。

## 5. 状态模型

### 5.1 用户状态

```text
NORMAL
FROZEN
```

### 5.2 电桩状态

```text
AVAILABLE
CHARGING
FAULT
OFFLINE
RESTARTING
```

### 5.3 订单状态

```text
CREATED
CHARGING
PENDING_PAYMENT
COMPLETED
CANCELLED
```

说明：

- `CREATED` 表示已选择电桩但尚未开始。
- `CHARGING` 表示正在充电。
- `PENDING_PAYMENT` 表示已停止但待结算。
- `COMPLETED` 表示已完成并结算。
- `CANCELLED` 表示取消或异常终止。

状态转换规则见 `BR-009`。

### 5.4 设备状态

```text
ONLINE
OFFLINE
FAULT
RESTARTING
```

## 6. 数据需求

V1 至少需要以下数据对象：

- User
- Admin
- ChargingStation
- ChargingPile
- ChargingOrder
- RechargeRecord
- Prediction
- DeviceLog
- OperationLog

数据库表名和字段名使用 snake_case。

Java / JSON 字段名使用 camelCase。

金额使用 `DECIMAL(10,2)` 和 `BigDecimal`。

时间使用 `DATETIME`，接口时间格式统一为 `yyyy-MM-dd HH:mm:ss`。

## 7. 外部接口需求

### 7.1 Qt TCP Socket 业务通信

Linux Qt 用户端和 Qt 管理端通过 TCP Socket 与服务端通信。

要求：

- Qt 业务页面不得直接操作 `QTcpSocket`。
- Qt 网络通信统一封装 `SocketClient` / `NetworkClient`。
- 应用层消息协议需统一定义请求类型、认证信息、请求数据、响应状态和错误信息。
- Socket 响应结构必须复用 REST API 的 `code`、`message`、`data` 语义，降低前后端联调成本。

### 7.2 REST API

Web 大屏和 Python ML 模块通过 REST API 与 Spring Boot 后端通信。

统一前缀：

```text
/api
```

API 必须使用统一返回结构：

```json
{
  "code": 200,
  "message": "success",
  "data": {}
}
```

### 7.3 腾讯地图

系统使用腾讯地图完成地址转经纬度和路线导航。

V1 推荐由后端代理地图 Key 和地理编码请求。

### 7.4 ML 集成

V1 可采用离线或定时 Python 任务：

```text
MySQL → Python preprocess/train/predict → MySQL prediction
```

不强制部署独立在线推理服务。

### 7.5 设备 Serial / TCP Socket

设备模拟器与设备接入层使用 Serial 或 TCP Socket。V1 优先实现 TCP Socket，串口可作为扩展。

设备协议详见 `docs/07-DEVICE-PROTOCOL.md`。

## 8. 非功能需求

### NFR-ENV-001 Linux 运行环境

Qt 用户端、Qt 管理端和主要服务应能在 Linux 虚拟机环境下运行。推荐环境为 Ubuntu 22.04+，最终版本在环境搭建阶段冻结。

### NFR-ENV-002 Qt 环境

Qt 开发环境推荐 Qt Creator 6.2+，最终版本在环境搭建阶段冻结。

### NFR-REL-001 通信稳定性

客户端请求失败、地图 API 失败、设备离线、ML 任务失败时，系统应给出明确错误提示或降级状态。

### NFR-ERR-001 完整错误处理

后端必须使用统一异常处理机制，业务错误应返回明确错误码和错误信息。

### NFR-SEC-001 数据安全

系统必须保护用户数据、管理员密码、地图 Key 和数据库连接信息，禁止明文密码和敏感配置进入 Git 仓库。

### NFR-THR-001 多线程结构

系统应体现合理多线程设计：

- Qt UI 线程不得被 Socket 请求、REST 请求或长任务阻塞。
- 设备通信使用独立 worker 或网关进程。
- ML 任务作为独立 Python 进程或脚本运行。

### NFR-MAINT-001 可维护性

系统应采用模块化目录结构，公共契约集中维护，避免无关重构和重复实现。

## 9. 安全需求

### SEC-001 管理员密码

管理员密码不得明文存储，推荐使用 BCrypt hash。

### SEC-002 敏感配置

数据库密码、地图 Key、认证密钥等不得提交到 Git 仓库。

### SEC-003 权限隔离

系统至少区分 `USER` 和 `ADMIN` 权限。

### SEC-004 文件上传

头像上传必须限制文件类型、大小和保存路径，禁止任意文件覆盖。

### SEC-005 身份认证

用户和管理员登录成功后获得认证凭证；后续受保护业务请求必须携带有效凭证。

要求：

- Qt Socket 业务消息必须支持携带认证凭证。
- Web / ML REST API 必须支持受保护接口认证。
- 用户和管理员至少区分 `USER` 与 `ADMIN` 权限。
- 具体凭证格式和实现机制在架构/API 设计阶段冻结，V1 建议采用轻量 JWT。

## 10. 验收标准

### 10.1 Demo Flow A：用户充电

```text
手机号登录
↓
定位
↓
附近充电站
↓
站点详情
↓
选择空闲桩
↓
开始充电
↓
实时充电
↓
停止
↓
计费
↓
钱包结算
↓
订单历史
```

### 10.2 Demo Flow B：运营管理

```text
Admin Login
↓
收入统计
↓
站点管理
↓
桩状态
↓
用户管理
↓
冻结用户
↓
远程重启设备
```

### 10.3 Demo Flow C：智能运营

```text
历史订单
↓
ML Training
↓
1h / 6h / 24h Prediction
↓
Prediction Result
├── 用户站点推荐
├── 管理端预警
└── Web 大屏
```

### 10.4 Demo Flow D：设备接入

```text
Device Simulator
↓
TCP Socket / Serial
↓
Device Gateway
↓
Backend
↓
Database
↓
Admin / Dashboard
```

## 11. 需求追踪矩阵

| 模块 | 核心需求 | Source | 依赖文档 |
| --- | --- | --- | --- |
| 用户端 | FR-U-001 至 FR-U-011，FR-C-001 至 FR-C-008 | Task Book + Team Derived | `docs/03-API.md`，`docs/04-DATABASE.md` |
| 管理端 | FR-A-001 至 FR-A-013 | Task Book + Team Derived | `docs/03-API.md`，`docs/04-DATABASE.md` |
| 大屏 | FR-D-001 至 FR-D-003 | Task Book + Team KPI Definition | `docs/03-API.md` |
| ML | FR-ML-001 至 FR-ML-006 | Task Book | `docs/04-DATABASE.md` |
| 设备 | FR-IOT-001 至 FR-IOT-006 | Task Book Constraint + Team Enhancement | `docs/07-DEVICE-PROTOCOL.md` |
| 多线程 | NFR-THR-001 | Task Book Technical Constraint | `docs/01-ARCHITECTURE.md` |
| 认证安全 | SEC-001 至 SEC-005 | Task Book + Team Security Rule | `docs/03-API.md` |

## 12. 待确认问题

### OI-001 SQLite 是否硬性要求

任务书中出现 QSQLite，但本项目主架构建议使用 MySQL 作为业务主数据库。若老师要求 SQLite，可将 SQLite 定义为 Qt 本地缓存或配置存储，不作为主业务数据库。

该问题属于架构 Blocker，必须在 SRS Freeze 前向老师确认。

### OI-002 Socket 使用范围与验收形式

当前 Freeze Candidate 假设 Qt 用户端与 Qt 管理端通过 TCP Socket 与后端进行核心业务通信，Web 与 ML 使用 REST API；设备接入模块另使用 TCP Socket / Serial。

需向老师确认：

1. Qt 主业务链路使用 TCP Socket 是否满足要求。
2. 是否要求所有网络通信均使用 Socket。
3. 答辩是否需要展示底层 Socket 连接、收发及多线程处理代码。

### OI-003 预约详细规则

V1 将预约简化为选择空闲桩后创建待启动订单，不实现复杂预约时间、排队、超时失效和取消规则。

### OI-004 天气数据来源

V1 使用模拟天气字段或预置历史数据，不强制接入真实天气 API。

### OI-005 大屏最终 KPI

V1 已给出推荐 KPI，后续可根据答辩重点微调。

## 13. 不纳入 V1 的内容

以下内容不纳入 V1：

- 微服务
- Kubernetes
- Kafka / RabbitMQ
- Redis Cluster
- Elasticsearch
- GraphQL
- 完整 OCPP 协议
- 复杂 RBAC
- 区块链
- 数字孪生
- LLM 客服

原因：这些内容会显著增加 5 人 1 周项目的集成风险，不能直接提升核心闭环完成度。
