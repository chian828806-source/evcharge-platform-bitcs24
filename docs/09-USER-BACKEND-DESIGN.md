# Qt 用户端后端设计 V1.0

状态：已实现并纳入统一 Qt Server 的 User Backend V1 设计与实现说明
适用范围：`qt-server` 中面向 Qt 用户端的业务后端  
最后更新：2026-09-02

## 1. 文档目的

本文档把用户端页面需求转换为可实施的服务端设计，回答以下问题：

- 每个页面操作发送什么 Socket 消息；
- 消息由哪个 Handler、Service 和 Repository 处理；
- 业务会读取或修改哪些 SQLite 表；
- 哪些操作必须使用数据库事务；
- 前后端联调前还需要确认哪些字段和交互。

本文档不是新的网络协议。公共消息外壳、错误码和认证规则以
`docs/03-API.md` 为准，数据库字段和事务规则以 `docs/04-DATABASE.md`
为准。本文提出但尚未写入 `docs/03-API.md` 的请求和响应字段均为
**待确认接口草案**；前后端确认后，必须先同步更新 `docs/03-API.md`
和 `shared/protocol`，再进入联调。

## 2. 设计依据与优先级

设计依据：

1. `docs/00-SRS-V1.0.md`：功能需求、业务规则和验收流程；
2. `docs/03-API.md`：TCP JSON Lines、Session、错误码和消息类型；
3. `docs/04-DATABASE.md`：表结构、状态枚举和事务规则；
4. `docs/08-EVCharge_Qt_UI_V2.0.md`：用户端页面、元素和数据来源；
5. `shared/protocol`：当前代码中已经登记的消息类型和错误码。

发生冲突时按以上顺序处理：先回到 SRS 确认需求，再修改 API 或数据库
公共契约，不能只在业务代码中私自改变规则。

## 3. 范围

### 3.1 V1 包含

- 手机号登录和自动注册；
- Session 创建和用户身份鉴权；
- 用户资料查询、昵称修改、头像上传；
- 钱包充值；
- 地址解析、附近站点、站点与电桩详情；
- 当前活动订单检查；
- 创建、开始、停止、取消和结算订单；
- 用户订单列表；
- 用户侧预测结果和推荐站点查询；
- 与上述功能对应的数据库事务、错误处理和测试。

### 3.2 V1 不包含

- Qt 用户端页面布局和控件样式；
- 管理员业务和管理端图表；
- Web 大屏 WebSocket 订阅；
- Python 模型训练实现；
- 真实支付平台、真实充电桩控制和生产级对象存储；
- REST API。

本设计遵循 `docs/01-ARCHITECTURE.md` 的三程序结构：`qt-user`、
`qt-admin` 和无界面的 `qt-server` 分别构建。`docs/08-EVCharge_Qt_UI_V2.0.md`
中“PC 端同时承担服务与管理端 UI”的旧表述不作为本设计的实现依据，后续应
单独修正文档，但不阻塞用户端后端开发。

## 4. 总体调用链

```text
Qt 用户端页面
  -> qt-user/network/SocketClient
  -> TCP JSON Lines
  -> qt-server/network/SocketServer + ClientSession
  -> MessageDispatcher（消息路由和公共 Session 鉴权）
  -> Handler（payload 校验和响应映射）
  -> Service（业务规则、状态机和事务意图）
  -> Repository（参数化 SQL 和数据映射）
  -> SQLite
```

各层只承担自己的职责：

| 层 | 负责 | 不负责 |
| --- | --- | --- |
| Handler | 读取 payload、校验字段类型、调用 Service、生成标准响应 | 拼接 SQL、决定复杂状态流转 |
| Service | 用户状态、订单归属、状态机、计费、事务和幂等 | 操作 Socket、绘制 UI |
| Repository | 参数化 SQL、查询结果映射、在指定事务中读写 | 网络 JSON、页面提示文案 |
| Database | 连接管理、`PRAGMA foreign_keys=ON`、事务边界、错误封装 | 用户业务规则 |
| Model/DTO | 层间传递结构化数据 | 数据库连接和 Socket 写入 |

用户身份一律来自 `MessageDispatcher` 验证后的 `SessionContext.principalId`。
除登录外，客户端不得通过 payload 自报 `userId`，Service 也不得相信这种字段。

## 5. 建议目录

```text
qt-server/
├── qt-server.pro
├── main.cpp
├── network/                         # 用户与管理员共用，保持业务无关
│
├── handlers/
│   ├── user/                        # 本文档负责的用户消息入口
│   │   ├── userhandler.h/.cpp
│   │   ├── stationhandler.h/.cpp
│   │   ├── orderhandler.h/.cpp
│   │   ├── predictionhandler.h/.cpp
│   │   ├── registeruserhandlers.h/.cpp
│   │   └── user-handlers.pri
│   └── admin/                       # 管理后端负责人维护
│       ├── registeradminhandlers.h/.cpp
│       └── admin-handlers.pri
├── services/
│   ├── user/                        # 本文档负责的用户业务规则
│   │   ├── userservice.h/.cpp
│   │   ├── walletservice.h/.cpp
│   │   ├── stationservice.h/.cpp
│   │   ├── orderservice.h/.cpp
│   │   ├── chargingservice.h/.cpp
│   │   └── predictionservice.h/.cpp
│   └── admin/                       # 管理后端负责人维护
│
├── repositories/                   # 双方共用，修改需共同评审
│   ├── userrepository.h/.cpp
│   ├── rechargerepository.h/.cpp
│   ├── stationrepository.h/.cpp
│   ├── pilerepository.h/.cpp
│   ├── orderrepository.h/.cpp
│   ├── predictionrepository.h/.cpp
│   └── operationlogrepository.h/.cpp
├── database/                       # 双方共用
│   ├── databaseconnection.h/.cpp
│   └── transactionrunner.h/.cpp
├── models/                         # 双方共用
│   ├── user.h
│   ├── station.h
│   ├── chargingpile.h
│   ├── chargingorder.h
│   └── prediction.h
├── adapters/                       # 双方共用的外部能力
│   ├── mapadapter.h/.cpp
│   └── avatarstorage.h/.cpp
├── workers/                        # 双方共用的后台任务
│   └── chargingprogressworker.h/.cpp
└── common/                         # ServiceResult、校验和通用类型
```

目录中的文件可随实现细化，但 `Handler -> Service -> Repository` 的依赖
方向不能反转。目录统一使用复数形式，不能再并行增加 `service/`、
`repository/`、`model/` 或 `worker/`。

### 5.1 用户后端负责范围

用户后端负责人主要维护：

```text
handlers/user/
services/user/
```

其中 `registerUserHandlers(...)` 集中注册本文件第 8 节的用户消息，并为每条
路由设置 Public 或 User 访问级别。用户 Handler 不得注册 `ADMIN_*` 消息，
用户 Service 不得依赖 `qt-admin` 或任何界面类。

### 5.2 管理后端负责范围

管理后端负责人主要维护：

```text
handlers/admin/
services/admin/
```

管理端的具体接口不在本文展开。它可以复用公共 Repository 和 Model，但不能
调用用户 Handler 绕过管理员路由权限，也不能把管理业务写进用户 Service。

### 5.3 双方共用范围

以下目录属于服务端公共层：

```text
network/
repositories/
database/
models/
workers/
adapters/
common/
```

两边可以按任务修改，但公共接口、SQL、字段映射、事务和线程模型发生变化时，
必须通知另一方并共同评审。不得分别创建两套 `UserRepository`、
`StationRepository`、数据库连接或订单状态定义。

`main.cpp` 只装配依赖并调用两个集中注册函数：

```cpp
registerUserHandlers(dispatcher, userDependencies);
registerAdminHandlers(dispatcher, adminDependencies);
```

用户和管理员模块分别维护自己的 `.pri` 文件，主 `qt-server.pro` 只引用这些
分组文件，减少两名开发者同时增加源码时产生 Git 冲突。

## 6. 页面到后端模块追踪

| UI | 用户操作 | 消息类型 | Handler | Service | Repository / 外部依赖 | 主要数据表 |
| --- | --- | --- | --- | --- | --- | --- |
| U01 | 手机号登录/自动注册 | `USER_LOGIN` | UserHandler | UserService | UserRepository、SessionManager | `user` |
| U02 | 地址转坐标 | `MAP_GEOCODE` | StationHandler | StationService | MapAdapter | 无 |
| U02 | 附近站点 | `STATION_LIST_NEARBY` | StationHandler | StationService | StationRepository、PileRepository | `charging_station`, `charging_pile` |
| U02 | 推荐站点 | `PREDICTION_RECOMMENDATION` | StationHandler | StationService | PredictionRepository、StationRepository、PileRepository | `prediction`, `charging_station`, `charging_pile` |
| U03 | 站点和电桩详情 | `STATION_DETAIL_GET` | StationHandler | StationService | StationRepository、PileRepository | `charging_station`, `charging_pile` |
| U03/U04 | 创建订单 | `ORDER_CREATE` | OrderHandler | OrderService | User/Order/Pile/Station Repository | `user`, `charging_order`, `charging_pile`, `charging_station` |
| U04 | 查询活动订单/刷新进度 | `ORDER_ACTIVE_CHECK` | OrderHandler | ChargingService | OrderRepository、StationRepository、PileRepository | `charging_order`, `charging_station`, `charging_pile`, `user` |
| U04 | 开始充电 | `ORDER_START` | OrderHandler | ChargingService | User/Order/Pile Repository | `user`, `charging_order`, `charging_pile` |
| U04 | 停止充电 | `ORDER_STOP` | OrderHandler | ChargingService | Order/Pile Repository | `charging_order`, `charging_pile` |
| U04 | 取消预约 | `ORDER_CANCEL` | OrderHandler | OrderService | Order/Pile Repository | `charging_order`, `charging_pile` |
| U04 | 钱包结算 | `ORDER_SETTLE` | OrderHandler | ChargingService | User/Order/Pile Repository | `user`, `charging_order`, `charging_pile` |
| U05 | 查询资料 | `USER_PROFILE_GET` | UserHandler | UserService | UserRepository | `user` |
| U05 | 修改昵称 | `USER_PROFILE_UPDATE` | UserHandler | UserService | UserRepository | `user` |
| U05 | 上传头像 | `USER_AVATAR_UPLOAD` | UserHandler | UserService | AvatarStorage、UserRepository | `user` + 头像文件 |
| U05 | 钱包充值 | `USER_RECHARGE` | UserHandler | WalletService | UserRepository、RechargeRepository | `user`, `recharge_record` |
| U05 | 查询订单列表 | `USER_ORDER_LIST` | OrderHandler | OrderService | OrderRepository、StationRepository、PileRepository | `charging_order`, `charging_station`, `charging_pile` |
| 可选详情 | 查询预测 | `PREDICTION_LIST` | PredictionHandler | PredictionService | PredictionRepository | `prediction` |

## 7. 公共数据约定

### 7.1 请求和响应外壳

所有请求和响应沿用 `docs/03-API.md`：

```json
{
  "requestId": "REQ-20260902-000001",
  "type": "USER_PROFILE_GET",
  "sessionId": "S-example",
  "payload": {}
}
```

```json
{
  "requestId": "REQ-20260902-000001",
  "code": 200,
  "message": "success",
  "data": {}
}
```

- `requestId` 必须原样返回；
- `code` 用于程序判断，`message` 只用于展示和日志；
- `data` 始终为对象，没有数据时返回 `{}`；
- 消息使用 UTF-8 紧凑 JSON，并以 LF 结尾；
- 除 `USER_LOGIN` 外，本文件中的消息均要求 USER Session。

### 7.2 字段命名和单位

- Socket JSON 使用 `camelCase`；
- SQLite 使用 `snake_case`；
- 金额使用整数“分”，字段名以 `Fen` 结尾；
- 电价使用 `priceFenPerKwh`，服务费使用 `serviceFeeFenPerKwh`；
- 综合展示单价使用 `totalPriceFenPerKwh = priceFenPerKwh + serviceFeeFenPerKwh`；
- 电量使用 `energyKwh`，功率使用 `powerKw`；
- 距离使用 `distanceKm`，保留两位小数；
- 时间来自 SQLite 时使用 `yyyy-MM-dd HH:mm:ss`；
- 经纬度使用 `longitude`、`latitude`，双方必须统一坐标系。

### 7.3 状态枚举

```text
UserStatus  = NORMAL | FROZEN
StationStatus = NORMAL | DISABLED
PileStatus  = AVAILABLE | RESERVED | CHARGING | FAULT | OFFLINE | RESTARTING
OrderStatus = CREATED | CHARGING | PENDING_PAYMENT | COMPLETED | CANCELLED
PileType    = FAST | SLOW
```

客户端只负责把枚举转换为中文显示，不能根据中文文本判断业务状态。

### 7.4 建议复用的响应对象

以下对象是接口草案，用于减少不同接口字段不一致：

```json
{
  "userId": 1,
  "phone": "13800138000",
  "nickname": "用户8000",
  "avatarPath": null,
  "balanceFen": 10000,
  "status": "NORMAL"
}
```

```json
{
  "stationId": 1,
  "stationNo": "ST001",
  "name": "软件园充电站",
  "address": "示例地址",
  "district": "甘井子区",
  "longitude": 121.500001,
  "latitude": 38.900001,
  "priceFenPerKwh": 80,
  "serviceFeeFenPerKwh": 30,
  "totalPriceFenPerKwh": 110,
  "status": "NORMAL",
  "totalPileCount": 4,
  "availablePileCount": 2,
  "distanceKm": 1.25
}
```

```json
{
  "orderId": 1001,
  "orderNo": "O202609020001",
  "stationId": 1,
  "stationName": "软件园充电站",
  "pileId": 1,
  "pileNo": "P01",
  "pileType": "FAST",
  "powerKw": 60.0,
  "status": "CHARGING",
  "priceFenPerKwh": 80,
  "serviceFeeFenPerKwh": 30,
  "startAt": "2026-09-02 16:00:00",
  "endAt": null,
  "chargeMinutes": 5,
  "chargeSeconds": 300,
  "energyKwh": 5.0,
  "amountFen": 550,
  "createdAt": "2026-09-02 15:58:00"
}
```

响应是否返回完整对象或精简对象，由前后端在联调前确认；相同含义的字段
必须保持同名、同类型和同单位。

## 8. 消息处理设计

本节字段是对现有消息类型的实现建议。未在 `docs/03-API.md` 中冻结的字段
必须先完成第 16 节的确认流程。

### 8.1 `USER_LOGIN`

| 项目 | 内容 |
| --- | --- |
| 权限 | Public |
| 请求 payload | `phone: string` |
| 成功 data | `sessionId`, `user` |
| 主要规则 | 11 位手机号；不存在则自动注册；冻结规则按 SRS 执行 |
| 数据操作 | 查询/新增 `user`，更新 `last_login_at` |

注意：当前项目是手机号免密登录，不能擅自增加密码字段。新用户默认昵称为
`用户` 加手机号后四位，初始余额为 0，状态为 `NORMAL`。登录成功后由
`SessionManager` 生成不可预测的 USER Session。

### 8.2 `USER_PROFILE_GET`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `{}` |
| 成功 data | `user` |
| 数据操作 | 按 Session 用户 ID 查询 `user` |

至少返回 `userId`、`phone`、`nickname`、`avatarPath`、`balanceFen` 和
`status`，不得返回数据库内部敏感字段。

### 8.3 `USER_PROFILE_UPDATE`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `nickname: string` |
| 成功 data | 更新后的 `user` 或 `nickname` |
| 数据操作 | 更新 `user.nickname`, `user.updated_at` |

昵称去除首尾空白后必须为 2 到 20 个字符。昵称非法时需要稳定的业务错误码，
但当前 `docs/03-API.md` 尚未定义，见第 16 节待确认项。

### 8.4 `USER_AVATAR_UPLOAD`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload 草案 | `fileName`, `mimeType`, `contentBase64` |
| 成功 data | `avatarPath`, `user` |
| 文件操作 | 服务端保存头像文件；路径由 `qt-server --avatar-dir` 配置 |
| 数据操作 | 更新 `user.avatar_path`, `user.updated_at` |

建议服务端流程：

1. 校验扩展名、MIME、解码结果和文件大小；
2. 生成服务端文件名，不直接使用客户端路径作为目标路径；
3. 保存到配置的头像目录；
4. 数据库只保存相对路径；
5. 数据库更新失败时删除本次新文件；
6. 成功替换后再清理旧头像。

V1 已冻结：仅允许 PNG 或 JPEG，文件名扩展名必须与 MIME 匹配，解码后的原始
文件最大 512 KiB。服务端以随机文件名保存，数据库只保存 `avatars/<文件名>`
形式的相对路径；旧头像在成功替换后清理。

用户端不拼接服务端磁盘路径，也不通过当前协议读取头像文件。服务端在上传时校验
文件扩展名、大小与图片签名，防止非法文件写入头像目录。

### 8.5 `USER_RECHARGE`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `amountFen: integer` |
| 成功 data | `rechargeId`, `recordNo`, `amountFen`, `balanceFen`, `createdAt` |
| 主要规则 | 金额为 1 到 100000000 分；用户必须为 `NORMAL`；模拟支付成功 |
| 数据操作 | 更新 `user.balance_fen`，插入 `recharge_record` |

增加余额和写入充值流水必须在同一个事务中完成。V1 在服务进程内按
`userId + requestId` 缓存成功响应；同一用户重复提交同一 `requestId` 不会再次
充值。服务重启后的跨进程持久化幂等仍属于 C10。

### 8.6 `USER_ORDER_LIST`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `page`（默认 1）、`pageSize`（默认 20，最大 50）、可选单个 `status` |
| 成功 data | `items`, `page`, `pageSize`, `total` |
| 排序建议 | `created_at DESC, id DESC` |
| 数据操作 | 查询当前用户的 `charging_order` 并关联站点、电桩 |

Service 必须强制使用 Session 用户 ID，不能查询其他用户的订单。`status` 仅允许
`CREATED`、`CHARGING`、`PENDING_PAYMENT`、`COMPLETED`、`CANCELLED` 中的一个值。

### 8.7 `MAP_GEOCODE`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload 草案 | `district`, `address` |
| 成功 data | `formattedAddress`, `longitude`, `latitude` |
| 外部依赖 | 腾讯地图 Web API 或 V1 Mock Adapter |
| 数据操作 | 无 |

已确认的接口边界：`MAP_GEOCODE` 完成后由服务端调用腾讯地图，`QWebEngineView` 仅依据服务端
返回的坐标或导航 URL 展示路线。真实 `MapAdapter` 仍为已知 TODO，不阻塞本轮合并。地图 Key 必须来自配置或环境变量，不得提交到
仓库；外部请求属于耗时操作，必须经异步 `MapAdapter`，不得阻塞 Socket 线程。

### 8.8 `STATION_LIST_NEARBY`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload 草案 | `longitude`, `latitude`，可选 `district`, `limit` |
| 成功 data | `stations` |
| 排序 | `distanceKm` 升序 |
| 数据操作 | 查询正常站点和电桩状态聚合 |

每个站点至少返回站名、价格、服务费、电桩总数、空闲数量和距离。空闲数量只
统计 `AVAILABLE`。距离使用统一的 Haversine 或平面近似算法，单位 km，
展示值保留两位小数。`DISABLED` 站点默认不向普通用户展示。

### 8.9 `STATION_DETAIL_GET`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `stationId: integer` |
| 成功 data | `station`, `piles` |
| 数据操作 | 查询 `charging_station`, `charging_pile` |

电桩至少返回 `pileId`、`pileNo`、`type`、`powerKw`、`status`。只有
`AVAILABLE` 状态在 UI 中允许点击“选择”，但服务端仍必须再次检查状态，
不能依赖按钮禁用保证安全。

### 8.10 `ORDER_ACTIVE_CHECK`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `{}` |
| 成功 data 草案 | `hasActiveOrder`, 有订单时返回 `order` 和 `balanceFen` |
| 活动状态 | `CREATED`, `CHARGING`, `PENDING_PAYMENT` |

V1 约定：U04 打开时请求一次；充电中每 1 秒轮询本消息；页面离开 U04 或订单
不再是 `CHARGING` 时停止轮询。服务端在响应中计算当前 `chargeSeconds`、
`chargeMinutes`、`energyKwh` 和预估 `amountFen`，因此不新增未经登记的 TCP 推送消息。

### 8.11 `ORDER_CREATE`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `pileId: integer` |
| 成功 data | 创建后的 `order` |
| 主要错误 | 4002、4101、4102、4202、5001 |

同一事务内：

1. 校验用户状态为 `NORMAL`；
2. 校验用户不存在活动订单；
3. 校验电桩和所属站点存在且可用；
4. 保存站点 ID、电价和服务费快照，创建 `CREATED` 订单；
5. 将电桩改为 `RESERVED` 并写入 `current_order_id`；
6. 任一步失败全部回滚。

### 8.12 `ORDER_CANCEL`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `orderId: integer`，可选 `reason` |
| 成功 data | 更新后的 `order` |
| 主要错误 | 4104、4105、5001 |

必须校验订单属于当前 Session 用户，且状态只能为 `CREATED`。同一事务内将
订单改为 `CANCELLED`、记录取消时间，并将匹配的电桩恢复为 `AVAILABLE`、
清空 `current_order_id`。

### 8.13 `ORDER_START`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `orderId: integer` |
| 成功 data | 更新后的 `order` |
| 主要错误 | 4002、4104、4102、5001 |

必须校验订单归属、订单为 `CREATED`、电桩为 `RESERVED`，且
`current_order_id` 与订单匹配。同一事务内写入 `start_at`，将订单和电桩
都改为 `CHARGING`。

### 8.14 `ORDER_STOP`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `orderId: integer` |
| 成功 data | 更新后的 `order`，必须包含最终 `energyKwh`, `amountFen` |
| 主要错误 | 4104、5001 |

必须校验订单属于当前用户且状态为 `CHARGING`。同一事务内计算并保存时长、
电量和金额，将订单改为 `PENDING_PAYMENT`，释放电桩并清空
`current_order_id`。冻结用户若已有充电订单，仍允许停止。

计费固定由服务端完成：

```text
amountFen = round(energyKwh *
                  (priceFenPerKwh + serviceFeeFenPerKwh))
```

客户端可以展示服务端返回的金额，但不得用另一套公式形成最终账单。

### 8.15 `ORDER_SETTLE`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload | `orderId: integer` |
| 成功 data | 更新后的 `order`, `balanceFen` |
| 主要错误 | 4103、4104、5001 |

必须校验订单归属。余额不足时不扣款，订单保持 `PENDING_PAYMENT`。余额足够时，
扣减余额、将订单改为 `COMPLETED`、写入 `paid_at`，并更新电桩累计统计；这些
写操作必须在同一个事务中完成。`COMPLETED` 订单重复结算直接返回已完成结果，
不得再次扣款。冻结用户已有待支付订单时仍允许结算。

### 8.16 `PREDICTION_LIST`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload 草案 | `stationId`，可选 `horizon` |
| 成功 data | `predictions` |
| 数据操作 | 查询 `prediction` 最新结果 |

无预测结果时使用公共错误码 4501。V1 用户端若不展示独立预测详情，可暂不在
第一阶段实现，但消息登记保持不变。

### 8.17 `PREDICTION_RECOMMENDATION`

| 项目 | 内容 |
| --- | --- |
| 权限 | User |
| 请求 payload 草案 | 当前坐标、可选 `limit`, `horizon` |
| 成功 data | `stations`，包含推荐依据 |
| 数据操作 | 查询最新预测并关联正常站点、电桩统计 |

V1 已冻结：默认查询 `1h` 预测窗口，仅返回当前可用且预测可用桩数大于 0 的正常
站点；按“预测负荷升序 → 距离升序 → 预测可用桩数降序”排序。默认 `limit=5`，
最大 20。每项返回 `recommended`、`predictedLoad`、
`predictedAvailablePileCount` 和 `recommendationReason`。

该消息归 User/Station 业务侧：`StationService` 组合预测数据、站点信息、距离以及
实时/预测可用桩形成用户推荐，因此不是纯预测数据查询。它只能由
UserBackendRegistry 的 StationHandler 注册；PredictionHandlerRegistry 仅负责
`PREDICTION_LIST` 和 `PREDICTION_WARNING`，两个 Registry 禁止同时注册
`PREDICTION_RECOMMENDATION`。

## 9. 业务状态机

订单允许的状态流转：

```text
CREATED -> CHARGING -> PENDING_PAYMENT -> COMPLETED
CREATED -> CANCELLED
PENDING_PAYMENT -> PENDING_PAYMENT  # 余额不足
```

电桩正常状态流转：

```text
AVAILABLE -> RESERVED -> CHARGING -> AVAILABLE
AVAILABLE -> RESERVED -> AVAILABLE  # 取消订单
```

Service 必须拒绝所有未列出的状态转换。普通用户不能取消 `CHARGING` 订单，
不能跳过停止直接结算，也不能直接修改电桩状态。

## 10. 充电进度计算

现有需求只确定“时长、电量、预估金额由服务端计算或推送”，尚未确定 V1
模拟电量算法。建议采用确定性、可测试的公式：

```text
elapsedSeconds = max(0, now - startAt)
energyKwh = powerKw * elapsedSeconds / 3600
estimatedAmountFen = round(energyKwh *
                           (priceFenPerKwh + serviceFeeFenPerKwh))
```

如果演示需要时间加速，只能通过服务端配置统一设置倍率；不能由客户端自行
放大电量。`ORDER_ACTIVE_CHECK` 返回过程估算，`ORDER_STOP` 保存最终快照。

该算法和是否启用演示倍率属于待确认项，确认后应写入 SRS/API 的统一口径。

## 11. 数据库与并发

- 每个数据库线程必须拥有独立命名的 `QSqlDatabase` 连接；
- 每次打开连接都执行 `PRAGMA foreign_keys = ON`；
- 所有 SQL 使用 `prepare` 和 `bindValue`，禁止拼接用户输入；
- Socket 读取回调不执行长事务或外部地图请求；
- 同一用户活动订单互斥和同一电桩占用互斥必须在写事务中再次查询；
- 写操作建议由单独 Database Worker 串行处理，或通过明确的锁和事务保证；
- 不跨线程共享 `QSqlDatabase`、`QSqlQuery` 或 `QTcpSocket`；
- 事务失败统一回滚，并转换为公共错误码 5001；
- 日志中可以记录 SQL 错误，但响应不得泄露 SQL、磁盘路径或密钥。

## 12. 重试与幂等

充值、创建订单、停止和结算不能因为客户端超时重试而重复生效。

V1 最低实现：

1. 服务端按 `sessionId + requestId + type` 保存已完成写请求的响应；
2. 同一请求再次到达时返回原响应，不再次调用 Service；
3. 同一 `requestId` 携带不同 `type` 或不同 payload 时返回协议错误；
4. 缓存设置数量或时间上限，防止无限增长；
5. 订单 Service 仍需通过状态机保证重复停止、结算不会重复写入；
6. 充值事务仍需独立幂等保护，不能只依赖余额检查。

如果要求服务重启后仍能识别旧请求，则需要增加持久化幂等记录；这会修改
数据库公共契约，必须先走 `docs/04-DATABASE.md` 的变更流程。

## 13. 错误处理

当前公共错误码：

| code | 典型使用场景 |
| --- | --- |
| 200 | 成功 |
| 4001 | 手机号格式非法 |
| 4002 | 用户被冻结 |
| 4003 | Session 缺失、无效或角色错误 |
| 4101 | 用户存在活动订单 |
| 4102 | 电桩不可用 |
| 4103 | 钱包余额不足 |
| 4104 | 非法订单状态 |
| 4105 | 订单不可取消 |
| 4201 | 站点不存在 |
| 4202 | 电桩不存在 |
| 4401 | 消息外壳或 payload 结构错误、未知 type |
| 4402 | Socket 请求超时 |
| 4501 | 预测结果不存在 |
| 5001 | 数据库错误 |
| 5002 | 系统内部错误 |

订单不存在、订单不属于当前用户、昵称非法、充值金额非法、头像非法和地图服务
失败目前没有独立公共错误码。不能在代码中随意选择新数字；前后端确认后先在
`docs/03-API.md` 与 `shared/protocol/errorcodes.h` 中统一增加。

为避免泄露其他用户订单是否存在，订单不存在和不属于当前用户建议对客户端
返回同一种“订单不可访问”错误。

## 14. 安全要求

- 手机号只用于登录和展示，不作为 Service 的可信身份；
- Session 使用不可预测随机值，用户与管理员角色严格隔离；
- 冻结状态由 Service 每次执行受限业务时检查，不能只在登录时检查；
- 地图 Key、数据库路径和头像根目录从配置读取，不写入仓库；
- 头像路径必须防止 `..`、绝对路径和扩展名伪造；
- 限制消息帧、分页大小、字符串长度和头像大小；
- 响应不得返回管理员密码哈希、数据库内部错误或服务器绝对路径；
- 服务日志对手机号和 Session 进行脱敏，不记录头像 Base64 正文。

## 15. 测试计划

### 15.1 Repository 测试

- 手机号查询、自动注册和唯一索引；
- 站点距离查询所需数据和空闲桩聚合；
- 用户订单分页与排序；
- 充值流水和余额一致；
- 订单与电桩状态同时更新；
- 所有写失败时事务回滚。

### 15.2 Service 测试

- 新手机号自动注册，重复登录不重复创建；
- 冻结用户限制以及活动订单收尾例外；
- 每个用户最多一个活动订单；
- 每个电桩最多一个占用订单；
- 非法订单状态转换全部拒绝；
- 停止时计费正确；
- 余额不足保持 `PENDING_PAYMENT`；
- 重复结算不重复扣款；
- 重复充值 requestId 不重复增加余额；
- 用户不能操作或查看其他用户的订单。

### 15.3 Handler 与集成测试

- 所有 Handler 注册到正确的 Message Type 和 Access；
- 公共登录不要求 Session，其余用户消息要求 USER Session；
- 缺字段、错类型、非对象 payload 返回稳定错误；
- requestId 在响应中原样返回；
- 完成真实 `SocketClient -> SocketServer -> Handler -> SQLite` 登录闭环；
- 完成“查站点 -> 下单 -> 开始 -> 停止 -> 结算 -> 查历史”主闭环；
- 拆包、粘包和多消息连续发送继续通过现有网络测试。

## 16. 编码前必须确认

以下项目未被当前公共契约完整冻结：

| 编号 | 待确认项 | 建议负责人 | 推荐默认方案 |
| --- | --- | --- | --- |
| C01 | 用户资料、站点、订单统一响应字段 | UI + 后端 | 采用第 7.4 节对象 |
| C04 | 模拟电量公式和演示加速倍率 | 后端 + 验收负责人 | 第 10 节公式，默认不加速 |
| C06 | 地图地址解析在客户端还是服务端 | UI + 后端 | 只保留一种实现 |
| C08 | 昵称、金额、头像、订单归属等错误码 | UI + 后端 | 统一补充到 `03-API.md` |
| C09 | Session 有效期、多端登录和主动注销 | UI + 后端 | V1 内存 Session，服务重启后重新登录 |
| C10 | 服务重启后的持久化幂等要求 | 后端 + 验收负责人 | V1 仅进程内缓存，订单状态机兜底 |

确认流程：

1. 前端逐页核对显示字段和按钮结果；
2. 后端核对表字段、状态机和事务；
3. 对待确认项形成结论；
4. 把最终请求/响应字段和错误码写入 `docs/03-API.md`；
5. 更新 `shared/protocol`；
6. 双方再开始真实接口联调。

## 17. 推荐实施顺序

### 阶段 1：基础数据能力

- DatabaseConnection 和事务封装；
- User、Station、Pile、Order Repository；
- Repository 自动化测试。

### 阶段 2：登录与资料闭环

- `USER_LOGIN`；
- `USER_PROFILE_GET`；
- `USER_PROFILE_UPDATE`；
- Handler 注册和 Session 联调。

### 阶段 3：站点查询闭环

- `STATION_LIST_NEARBY`；
- `STATION_DETAIL_GET`；
- `MAP_GEOCODE` 契约已确认；待配置腾讯地图 Key 与异步 `MapAdapter` 后实现真实调用。

### 阶段 4：充电主闭环

- `ORDER_ACTIVE_CHECK`；
- `ORDER_CREATE`；
- `ORDER_START`；
- `ORDER_STOP`；
- `ORDER_SETTLE`；
- `ORDER_CANCEL`；
- 状态机、事务、并发和幂等测试。

### 阶段 5：个人中心补全

- `USER_RECHARGE`；
- `USER_ORDER_LIST`；
- `USER_AVATAR_UPLOAD`；

### 阶段 6：智能推荐

- `PREDICTION_LIST`；
- `PREDICTION_RECOMMENDATION`；
- 与 ML 输出字段和 UI 推荐标识联调。

## 18. 完成标准

用户端后端 V1 完成必须同时满足：

- 所有已实现消息在 `MessageDispatcher` 中注册正确权限；
- Handler、Service、Repository 没有跨层职责；
- Qt 用户端和管理端都不直接访问 SQLite；
- 用户身份只来自 SessionContext；
- 主订单流程符合 SRS 状态机；
- 充值、下单、停止和结算具备事务与重复请求保护；
- 金额、时间、坐标、状态字段与公共文档一致；
- 自动化测试和至少一次真实 Socket 主流程联调通过；
- `docs/03-API.md` 已记录最终接口字段，不再依赖口头约定；
- UI 文档第 8 节的接口联调检查逐项通过。
