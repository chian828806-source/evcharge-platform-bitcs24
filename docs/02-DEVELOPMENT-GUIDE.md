# 开发规范

本文档以 `docs/00-SRS-V1.0.md` 为需求基线，约束编码、配置、日志、线程、测试和模块实现方式。

## 1. 通用原则

1. 所有实现必须能追踪到 SRS 需求编号。
2. 先完成 P0 / MUST，再投入 P1 / SHOULD。
3. 不为简单功能引入新框架。
4. 不重构无关模块。
5. 不直接修改公共契约，契约包括 Socket 业务消息、REST API、数据库、状态枚举和设备协议。

## 2. 后端分层

统一采用：

```text
Controller
    ↓
Service
    ↓
Mapper
    ↓
MySQL
```

推荐目录：

```text
backend/src/main/java/.../

├── common/
├── config/
├── controller/
├── service/
│   └── impl/
├── mapper/
├── entity/
├── dto/
├── vo/
├── exception/
└── util/
```

### 2.1 Controller

Controller 只负责接收请求、参数校验、调用 Service 和返回结果。

Controller 禁止：

- 编写复杂业务逻辑
- 直接操作 Mapper
- 编写 SQL
- 直接修改数据库

### 2.2 Service

核心业务逻辑必须放在 Service。

涉及多个持久化操作且需要保持业务一致性的操作必须使用事务，例如：

- 用户自动注册
- 用户充值
- 开始充电
- 停止充电
- 钱包结算
- 新增电站和自动建桩
- 远程重启状态落库

### 2.3 Mapper

Mapper 只负责数据库访问。

简单 CRUD 优先使用 MyBatis-Plus；复杂统计可以自定义 SQL，但禁止在多个位置重复实现相同 SQL。

## 3. Entity / DTO / VO

Entity 对应数据库表，原则上只用于数据库层。

DTO 表示客户端到后端的输入，例如：

```text
LoginDTO
RechargeDTO
CreateStationDTO
StartChargingDTO
FinishChargingDTO
```

VO 表示后端到客户端的输出，例如：

```text
UserVO
StationVO
ChargerVO
RevenueVO
PredictionVO
```

禁止为了省事直接把 Entity 作为所有请求和响应对象。

## 4. 状态值

代码中禁止出现无法理解的裸数字状态值。

统一状态见 SRS：

- `UserStatus`: `NORMAL`, `FROZEN`
- `PileStatus`: `AVAILABLE`, `CHARGING`, `FAULT`, `OFFLINE`, `RESTARTING`
- `OrderStatus`: `CREATED`, `CHARGING`, `PENDING_PAYMENT`, `COMPLETED`, `CANCELLED`
- `DeviceStatus`: `ONLINE`, `OFFLINE`, `FAULT`, `RESTARTING`

状态值一旦进入联调，修改必须走公共契约变更流程。

## 5. 时间与金额

数据库时间字段使用 `DATETIME`。

接口时间格式：

```text
yyyy-MM-dd HH:mm:ss
```

金额禁止使用 `float` 或 `double` 做最终业务计算。

Java 使用：

```text
BigDecimal
```

MySQL 使用：

```text
DECIMAL(10,2)
```

## 6. 配置管理

敏感配置不得提交到 Git。

包括：

- 数据库密码
- 腾讯地图 Key
- JWT Secret
- 上传目录绝对路径
- 生产环境配置

推荐：

- `application.yml` 保存非敏感默认配置。
- `application-local.yml` 或环境变量保存本机配置。
- 仓库只提交 `application-example.yml`。

## 7. 日志规范

后端、设备网关和 ML 模块都必须记录关键日志。

至少记录：

- 登录失败
- 自动注册
- 充值
- 开始充电
- 停止充电
- 结算失败
- 管理员操作
- 用户冻结 / 解冻
- 设备上线 / 离线
- 设备故障
- 远程重启
- ML 任务异常

日志中不得输出明文密码、Token、数据库密码或地图 Key。

## 8. 错误处理

后端统一使用：

```text
BusinessException
GlobalExceptionHandler
Result<T>
```

业务错误必须返回明确错误码和错误信息。

禁止返回纯字符串表示失败，也禁止只使用 `System.out.println` 处理错误。

## 9. 文件上传

头像上传 V1 推荐保存到：

```text
backend/uploads/avatar/
```

数据库保存：

```text
avatar_url
```

必须限制文件类型、文件大小、保存路径和文件名冲突。

禁止允许上传文件覆盖任意服务器路径。

## 10. Qt 开发规范

类名使用 PascalCase，函数名使用 camelCase，成员变量统一使用 `m_` 前缀。

示例：

```text
LoginWindow
StationPage
ChargingPage
OrderPage
NetworkClient

loadStations()
startCharging()
updateUserInfo()

m_userId
m_stationList
m_networkManager
```

Qt UI 类和业务逻辑适度分离。不要把 Socket 通信、消息解析、页面绘制和业务判断全部写进一个按钮槽函数。

## 11. Qt 网络与线程

Qt 网络通信统一封装：

```text
SocketClient / NetworkClient
```

负责：

```text
连接管理
请求发送
响应解析
错误处理
重连或超时处理
```

业务页面不得直接操作 `QTcpSocket`。

UI 线程不得被 Socket 请求、地图加载、设备命令或长计算阻塞。

涉及耗时操作时，使用 Qt 异步网络机制、`QThread` 或 `QtConcurrent`。

Qt 业务通信使用 `QTcpSocket`。服务端 Socket 接入可使用 Java Socket / Netty / Spring Integration 等轻量方案，具体实现需在后端工程创建时确认。

## 12. Web 开发规范

所有业务数据必须来自 API。

推荐目录：

```text
web-dashboard/

├── index.html
├── css/
├── js/
│   ├── api.js
│   ├── charts.js
│   └── main.js
└── assets/
```

`api.js` 负责访问后端，`charts.js` 负责图表配置。

## 13. ML 模块规范

推荐目录：

```text
ml/

├── data/
├── models/
├── src/
│   ├── preprocess.py
│   ├── train.py
│   ├── predict.py
│   └── database.py
├── requirements.txt
└── README.md
```

禁止把训练、测试、清洗、绘图全部堆在一个 `.py` 文件。

预测结果必须进入数据库或通过后端 API 进入系统。

## 14. Device 模块规范

推荐目录：

```text
device/

├── gateway/
└── simulator/
```

协议必须遵循 `docs/07-DEVICE-PROTOCOL.md`。

不得自行增加未记录的消息类型、状态值或字段。

## 15. 测试规范

每个 P0 功能至少完成基本自测。

优先测试：

- 手机号登录与自动注册
- 冻结用户限制
- 充值
- 未完成订单拦截
- 开始充电
- 停止充电
- 钱包结算
- 管理员登录
- 站点和电桩管理
- API 统一返回
- ML 预测结果写回
- 设备心跳和远程重启

## 16. Definition of Ready

一个任务开始开发前至少应明确：

- 对应 SRS 需求编号。
- 输入和输出。
- 涉及 API。
- 涉及数据表。
- 禁止修改范围。
- 验收标准。

## 17. Definition of Done

一个功能 Done 至少要求：

- 代码完成。
- 本地可运行。
- API 联通。
- 数据正确。
- 基本异常处理完成。
- 必要测试通过。
- 文档同步更新。
- 已提交 Git。
- 已合并或准备合并 `develop`。

## 18. 禁止事项

1. 未讨论自行修改数据库结构。
2. 未通知自行修改 API。
3. 客户端直接操作核心数据库。
4. `main` 分支直接开发。
5. 提交无法编译的代码。
6. Agent 大规模重写已有模块。
7. 为简单功能随意添加新框架。
8. 最后一天才第一次联调。
9. 只在个人电脑能够运行。
10. ML、大屏等模块使用完全脱离系统的假数据作为最终成果。
