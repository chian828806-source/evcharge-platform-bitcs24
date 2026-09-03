# Qt 管理端后端设计 V1.0

状态：Draft（管理端联调对齐稿）  
适用范围：`qt-server` 中管理员业务和 `qt-admin` 的接口调用  
最后更新：2026-09-03

## 1. 文档目的

本文档只整理管理端，不涉及用户端。主要说明：

- 管理页面分别调用哪些 Socket 消息；
- 服务端已有逻辑和返回字段；
- 哪些 UI 操作需要预留接口；
- 管理端 UI 必须预留哪些控件和状态。

公共消息格式和错误码以 `docs/03-API.md` 为准，数据库结构以
`docs/04-DATABASE.md` 为准。接口字段修改后，应同时更新公共协议和本文件。

## 2. 当前结构

```text
qt-admin 页面
  -> AdminSocketClient
  -> TCP JSON Lines
  -> qt-server MessageDispatcher
  -> handlers/admin 或 handlers/prediction
  -> services/admin 或 services/prediction
  -> Repository
  -> SQLite
```

职责边界：

- `qt-admin` 负责显示、输入、按钮状态和操作提示；
- `AdminSocketClient` 负责连接、发送请求、超时和响应接收；
- `qt-server` 负责权限、数据校验、统计、状态修改和数据库事务；
- 管理端页面不直接访问 SQLite，也不自行修改业务状态。

## 3. 页面与接口对应关系

| 页面 | 操作 | 消息类型 | 请求 | 主要返回字段 | 当前状态 |
| --- | --- | --- | --- | --- | --- |
| A01 登录 | 管理员登录 | `ADMIN_LOGIN` | `username`, `password` | `sessionId`, `admin` | 已接入 |
| A02 概览 | 查询营收汇总 | `ADMIN_REVENUE_SUMMARY` | 空 | `todayRevenueFen`, `monthRevenueFen`, `totalRevenueFen` | 已接入 |
| A02 概览 | 查询 7/30 日趋势 | `ADMIN_REVENUE_TREND` | `days` | `days`, `points[]` | 已接入 |
| A02 概览 | 查询电桩状态 | `ADMIN_PILE_STATUS_SUMMARY` | 空 | `total`, `statuses[]` | 已接入 |
| A02 概览 | 查询负荷预警 | `PREDICTION_WARNING` | `horizon`, `limit` | `predictions[]` | 已接入 |
| A03 电桩 | 查询全部或指定站点电桩 | `ADMIN_PILE_LIST` | 可选 `stationId` | `piles[]` | 已接入 |
| A03 电桩 | 模拟远程重启 | `ADMIN_PILE_RESTART` | `pileId` | `status`, `restoreStatus` | 已接入 |
| A04 站点 | 查询站点 | `ADMIN_STATION_LIST` | 空 | `stations[]` | 已接入 |
| A04 站点 | 新增站点和模拟电桩 | `ADMIN_STATION_CREATE` | 站点表单字段 | `stationId`, `stationNo`, `pileCount` | 已接入 |
| A04 站点 | 查看站内电桩 | `ADMIN_PILE_LIST` | `stationId` | `piles[]` | 已接入，目前跳到电桩页 |
| A05 用户 | 查询或按手机号搜索 | `ADMIN_USER_LIST` | 可选 `phoneKeyword` | `users[]` | 已接入 |
| A05 用户 | 冻结/解冻 | `ADMIN_USER_FREEZE` / `ADMIN_USER_UNFREEZE` | `userId` | `status`, `changed` | 已接入 |

## 4. 各页面接口细节

### 4.1 A01 管理员登录

请求：

```json
{"type":"ADMIN_LOGIN","payload":{"username":"admin","password":"123456"}}
```

成功后返回管理员信息和 `sessionId`。后续管理请求都必须携带该 Session。

UI 需要处理：

- 账号、密码为空时不发送请求；
- 连接中和验证中禁用重复登录；
- 登录失败后恢复按钮；
- 网络断开或 Session 失效后回到登录页；
- 登录后在顶部显示管理员 `displayName`，并提供退出登录按钮。

Session 失效或连接断开时，当前代码会清空 Session 并返回登录页，不要求用户重启程序。

### 4.2 A02 运营概览

#### 营收卡片

`ADMIN_REVENUE_SUMMARY` 返回三个金额，单位均为分。UI 转换为元并保留两位小数。

#### 营收趋势

`ADMIN_REVENUE_TREND` 只接受 `days=7` 或 `days=30`。每个点包含：

```text
date, revenueFen, energyKwh, orderCount
```

当前趋势区同时使用 `date`、`revenueFen`、`energyKwh` 和 `orderCount`：折线图显示三组
数据，图表下方明细表显示每天的完整数值。

#### 电桩状态

`ADMIN_PILE_STATUS_SUMMARY` 返回：

```text
total, statuses [{ status, count, ratio }]
```

UI 必须把英文状态转换为统一的中文名称和颜色，例如空闲、预约、充电中、故障、离线、重启中。

#### 负荷预警

管理端已接入 `PREDICTION_WARNING`。Dashboard 必须保留预警列表，至少显示：

```text
stationName, predictionTime, horizon, predictedLoad,
predictedAvailableCount, peakLevel
```

预警范围支持 `1h`、`6h`、`24h`，默认 `1h`，使用三个切换按钮。

### 4.3 A03 电桩管理

列表已有字段：

```text
pileId, pileNo, stationId, stationName, type, powerKw,
status, totalChargeCount, totalChargeMinutes
```

`pileId` 是操作使用的内部 ID，不一定需要直接显示。远程重启对 `RESERVED`、
`CHARGING`、`RESTARTING` 状态禁用。请求成功后状态会短暂变为 `RESTARTING`，
约 1.5 秒后恢复原状态。

前端应预留：

- 站点筛选和“查看全部”；
- 状态中文显示和颜色；
- 重启前确认弹窗；
- 请求期间按钮禁用；
- 成功提示以及自动刷新后的最终状态。

### 4.4 A04 充电站管理

站点列表字段：

```text
stationId, stationNo, name, address, longitude, latitude,
pileCount, onlineRate
```

新增站点请求字段：

```text
name, address, longitude, latitude, pileCount,
priceFenPerKwh（必填，界面默认 120）
```

新增弹窗已增加电价输入框。界面使用“元/度”，发送时转换为“分/度”；默认值为
1.20 元/度。

“查看电桩”采用左右分栏：左侧为站点列表，右侧显示所选站点的电桩详情。后端复用
`ADMIN_PILE_LIST { stationId }`，无需增加新接口。

### 4.5 A05 用户管理

列表字段：

```text
userId, phone, nickname, balanceFen, createdAt, status
```

当前只支持手机号部分匹配，没有分页、状态筛选或昵称搜索。V1 UI 不应先画出这些控件，
除非双方决定扩展接口。

前端应预留：

- 手机号输入和明确的查询按钮或回车查询；
- 清空搜索后恢复全部用户；
- 冻结/解冻确认弹窗；
- 操作期间按钮禁用；
- `changed=false` 时提示“状态未变化”，不要当成失败。

## 5. 通用页面状态

所有管理页面都必须支持以下状态：

| 状态 | UI 表现 | 后端配合 |
| --- | --- | --- |
| 首次加载 | 页面骨架或“正在加载” | 请求对应列表/统计接口 |
| 加载成功且有数据 | 显示表格或图表 | 返回标准 `data` |
| 加载成功但无数据 | 显示“暂无数据” | 返回空数组，不作为错误 |
| 请求失败 | 保留旧数据并显示失败提示 | 返回 `code/message` |
| 请求超时 | 提示重试 | `AdminSocketClient` 当前为 5 秒超时 |
| 连接断开 | 停止自动刷新并回登录页 | 清理 Session 和未完成请求 |
| 操作提交中 | 禁用对应按钮，避免重复提交 | 根据 `requestId` 匹配响应 |

## 6. 当前代码中需要补齐的管理端逻辑

| 优先级 | 内容 | 说明 |
| --- | --- | --- |
| 已完成 | Session 失效返回登录页 | 清空 Session 并重新显示登录页 |
| 已完成 | 操作按钮请求中状态 | 重启、建站、冻结/解冻等待响应时禁止重复点击 |
| 已完成 | 操作成功提示 | 重启、建站、冻结/解冻成功后给出提示并刷新数据 |
| 已完成 | 负荷预警接入 | 已调用 `PREDICTION_WARNING`，支持 1h/6h/24h |
| 已完成 | 站内电桩右侧详情 | 站点列表在左，所选站点电桩在右 |
| 已完成 | 新增站点电价 | 弹窗按元/度输入，请求按分/度发送 |
| 已完成 | 趋势完整字段 | 折线图和明细表使用日期、营收、电量、订单数 |
| P1 | Dashboard 局部失败处理 | 三个并行请求中一个失败时，不应影响其他卡片或图表 |
| P1 | 当前站点筛选提示 | 从站点页跳到电桩页后，应显示正在查看哪个站点并可清除筛选 |
| P1 | 空列表和加载状态 | 当前表格直接清空，用户无法区分加载中、无数据和失败 |
| P1 | 状态、类型和时间中文格式 | 当前多数直接显示服务端英文枚举和原始时间 |

## 7. 不在本文范围内

- 用户端登录、充电、钱包、订单和地图页面；
- 用户端接口及 `docs/09-USER-BACKEND-DESIGN.md` 的内容；
- Web 大屏布局；
- 机器学习训练过程；
- 真实充电桩控制。
