# 管理端前后端接口同步说明（2026-09-04）

## 1. 分支与改动范围

- 后端/联调分支：`fix/admin-unified-server-review`
- 基线分支：`develop`
- TCP 服务端默认地址：`127.0.0.1:18080`
- 本次没有增加或删除消息类型；现有协议可以直接对接。
- `ADMIN_STATION_CREATE.priceFenPerKwh` 增加服务端范围校验：1～10000 分/度。
- Admin 服务内部已改为通过 `DatabaseManager` 获取数据库连接。此项不改变前端字段。

本分支同时修改了以下 UI 文件，前端同学继续开发或合并时需要留意冲突：

```text
qt-admin/mainwindow.cpp
qt-admin/mainwindow.h
qt-admin/ui/adminpages.cpp
qt-admin/ui/adminpages.h
```

## 2. 公共请求和响应

登录以外的管理端请求都需要携带管理员 `sessionId`。

请求：

```json
{
  "requestId": "REQ-唯一值",
  "type": "ADMIN_REVENUE_SUMMARY",
  "sessionId": "S-登录后返回",
  "payload": {}
}
```

响应：

```json
{
  "requestId": "与请求相同",
  "code": 200,
  "message": "success",
  "data": {}
}
```

前端必须保存每个请求的 `requestId`。特别是 `ADMIN_PILE_LIST` 同时用于电桩管理页和
站点右侧详情，需要根据 `requestId` 把响应送到正确位置，不能只按消息类型分发。

## 3. 管理员登录信息

### 请求

```json
{
  "type": "ADMIN_LOGIN",
  "payload": {
    "username": "admin",
    "password": "123456"
  }
}
```

### 成功数据

```json
{
  "sessionId": "S-...",
  "admin": {
    "adminId": 1,
    "username": "admin",
    "displayName": "系统管理员"
  }
}
```

UI 使用约定：

- 顶部管理员区域显示 `displayName`，为空时显示 `username`；
- 退出登录由客户端清空 Session 并断开连接，不需要新接口；
- `code=4003` 或连接断开时返回登录页，不要求用户重启程序；
- 返回登录页时清空密码，服务器地址和端口可以保留。

## 4. 营收趋势

消息：`ADMIN_REVENUE_TREND`

请求只允许：

```json
{"days": 7}
```

或：

```json
{"days": 30}
```

响应字段：

```json
{
  "days": 7,
  "points": [
    {
      "date": "2026-09-04",
      "revenueFen": 128600,
      "energyKwh": 1018.5,
      "orderCount": 63
    }
  ]
}
```

字段用途：

| 字段 | UI 用途 |
| --- | --- |
| `date` | 横轴日期 |
| `revenueFen` | 除以 100 后显示元 |
| `energyKwh` | 显示充电量，单位 kWh |
| `orderCount` | 显示当日已完成订单数 |

本分支已经让图表使用三组数据，并保留 7/30 日选择。前端若调整图表样式，不要丢弃
`date`、`energyKwh` 或 `orderCount`。

## 5. 负荷预警

消息：`PREDICTION_WARNING`，仅管理员可调用。

请求：

```json
{
  "horizon": "1h",
  "limit": 20
}
```

- `horizon` 支持 `1h`、`6h`、`24h`；
- `limit` 范围 1～100；
- UI 默认使用 `1h`，切换后自动刷新时保持当前选择。

响应：

```json
{
  "predictions": [
    {
      "predictionId": 1,
      "batchId": "BATCH-001",
      "stationId": 2,
      "stationName": "万达广场充电中心",
      "predictionTime": "2026-09-04 18:00:00",
      "horizon": "1h",
      "predictedLoad": 0.91,
      "predictedAvailableCount": 1,
      "peakLevel": "HIGH",
      "modelName": "model-name",
      "mae": 0.05,
      "rmse": 0.08,
      "generatedAt": "2026-09-04 17:00:00"
    }
  ]
}
```

管理端预警列表至少展示：`stationName`、`predictionTime`、`horizon`、
`predictedLoad`、`predictedAvailableCount`、`peakLevel`。`predictedLoad` 为 0～1，
UI 转成百分比。返回空数组表示暂无预警，不是请求失败。

## 6. 站点与右侧电桩详情

先通过 `ADMIN_STATION_LIST` 获得：

```text
stationId, stationNo, name, address, longitude, latitude, pileCount, onlineRate
```

点击某个站点后调用：

```json
{
  "type": "ADMIN_PILE_LIST",
  "payload": {"stationId": 2}
}
```

响应：

```json
{
  "piles": [
    {
      "pileId": 7,
      "pileNo": "P001",
      "stationId": 2,
      "stationName": "万达广场充电中心",
      "type": "FAST",
      "powerKw": 60.0,
      "status": "AVAILABLE",
      "totalChargeCount": 126,
      "totalChargeMinutes": 3820
    }
  ]
}
```

布局和绑定约定：

- 站点表格在左，所选站点电桩详情在右；
- 不跳转到独立电桩管理页；
- 右侧至少显示桩号、类型、功率和状态；
- 新请求返回前保留旧内容并显示加载状态；
- 空数组显示“该站暂无电桩”；
- 连续点击不同站点时以各自 `requestId` 判断响应归属。

省略 `stationId` 调用相同接口时表示获取全部电桩，用于独立的电桩管理页。

## 7. 新增站点

消息：`ADMIN_STATION_CREATE`

```json
{
  "name": "软件园智慧充电站",
  "address": "软件园路 8 号",
  "longitude": 121.538,
  "latitude": 38.889,
  "pileCount": 4,
  "priceFenPerKwh": 120
}
```

| 字段 | 规则 |
| --- | --- |
| `name` | 非空字符串 |
| `address` | 非空字符串 |
| `longitude` | -180～180 |
| `latitude` | -90～90 |
| `pileCount` | 1～100 的整数 |
| `priceFenPerKwh` | 1～10000 的整数；UI 按元/度输入后乘 100 |

`priceFenPerKwh` 在协议上仍可省略，省略时服务端使用 120；当前管理端 UI 会始终发送。

## 8. 其余管理接口

| 消息 | 关键字段 | UI 注意事项 |
| --- | --- | --- |
| `ADMIN_REVENUE_SUMMARY` | `todayRevenueFen`, `monthRevenueFen`, `totalRevenueFen` | 分转元 |
| `ADMIN_PILE_STATUS_SUMMARY` | `total`, `statuses [{status,count,ratio}]` | `ratio` 转百分比 |
| `ADMIN_PILE_LIST` | 可选 `stationId` | 省略表示全部电桩 |
| `ADMIN_PILE_RESTART` | 请求 `pileId`；返回 `status`, `restoreStatus` | 预约、充电中、重启中不可重启 |
| `ADMIN_USER_LIST` | 可选 `phoneKeyword`；返回 `users[]` | 空关键词表示全部用户 |
| `ADMIN_USER_FREEZE` | 请求 `userId`；返回 `status`, `changed` | `changed=false` 仍是成功 |
| `ADMIN_USER_UNFREEZE` | 请求 `userId`；返回 `status`, `changed` | `changed=false` 仍是成功 |

重启、冻结和解冻都需要二次确认；写请求发出后禁用对应操作，收到成功、失败或超时后
恢复。成功后显示提示并刷新相关列表。

## 9. 本次不修改的协议

- 没有增加管理员退出消息；
- 没有增加专门的站点电桩详情消息，继续复用 `ADMIN_PILE_LIST`；
- 没有增加服务端状态筛选或分页；电桩状态筛选由 UI 对当前列表本地完成；
- 没有修改任何用户端消息；
- 字段不足时先与 Admin 后端负责人确认，不要直接修改 `shared/protocol` 或
  `docs/03-API.md`。
