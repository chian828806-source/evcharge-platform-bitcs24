# Web Dashboard

`web-dashboard/` 是 EVCharge 的浏览器运营展示模块。它只负责将服务端 WebSocket 推送转换为页面指标、表格和 ECharts 数据；不访问 SQLite、不包含 Qt 业务逻辑，也不重新实现服务端。

## 当前完成内容

- `DashboardWebSocketClient`：连接 `/dashboard`、发送 `DASHBOARD_SUBSCRIBE`、处理 `DASHBOARD_UPDATE`、安全忽略非法消息、指数退避重连并在重连后自动订阅。
- `DashboardStore`：集中保存四个 topic、连接状态、最后更新时间和最后一次有效数据。断线不会清空页面数据。
- `DashboardController`：将 Store 数据绑定到指标、筛选器、表格和交互入口。
- `DashboardCharts`：页面加载本地 `vendor/echarts.esm.min.js`，每个实例只初始化一次，后续只调用 `setOption()`；空数据会清除旧图，由页面 HTML 统一提示，缺失字段和 resize 都安全处理。
- 可关闭 Mock 模式：所有演示数据集中在 `mock/dashboard-mock.js`，不散落在图表或页面代码中。
- `tests/dashboard-logic-tests.html`：不依赖前端框架的浏览器测试页，覆盖 Store、订阅、消息路由、Mock、Charts 初始化、更新、空数据和 resize。

本模块刻意不进行视觉设计，只提供基础 HTML/CSS 结构以验证功能。

## 运行

浏览器模块应通过本地静态服务器打开，避免 `file://` 对 ES module 的限制：

```bash
cd web-dashboard
python3 -m http.server 8080
```

访问 `http://localhost:8080/`。默认是 Mock 模式；测试页地址为：

```text
http://localhost:8080/tests/dashboard-logic-tests.html
```

## 配置

配置集中在 `js/config.js`。默认地址是与当前页面主机相同、端口 `18081` 的：

```text
ws://<server-host>:18081/dashboard
```

真实 Qt 服务端联调时关闭 Mock：

```text
http://<dashboard-host>:8080/?mock=0&host=<server-host>&port=18081
```

也可以在加载 `js/app.js` 前设置 `window.EVCHARGE_DASHBOARD_CONFIG`，覆盖 `mockEnabled`、`host`、`port` 或完整 `websocketUrl`。不要把开发机器 IP 写入源码。

## WebSocket 协议

协议严格沿用 `docs/03-API.md` 和 `DashboardWebSocketServer`：

```json
{
  "requestId": "DASH-...",
  "type": "DASHBOARD_SUBSCRIBE",
  "payload": {
    "topics": ["summary", "pileStatus", "revenueTrend", "prediction"]
  }
}
```

服务端更新 envelope：

```json
{
  "type": "DASHBOARD_UPDATE",
  "topic": "summary",
  "data": {}
}
```

未知 type/topic、非对象 data 或非法 JSON 只记录并忽略，不会破坏已展示的有效数据。

## Dashboard V1 数据形状

`summary` 的前三个字段来自既有 WebSocket 示例；`totalOrderCount` 是 Web Dashboard V1 的可选展示字段，缺失时显示 `—`。真实 Qt 联调前必须最终确认服务端是否提供该字段：

```json
{
  "todayEnergyKwh": 128.5,
  "todayRevenueFen": 93600,
  "totalOrderCount": 42,
  "stationLoad": 0.62
}
```

`pileStatus` 使用既有状态枚举的计数对象：

```json
{
  "counts": {
    "AVAILABLE": 12, "RESERVED": 2, "CHARGING": 8,
    "FAULT": 1, "OFFLINE": 1, "RESTARTING": 0
  }
}
```

`revenueTrend` 的每一项同时包含日期、充电量和以分为单位的营收。浏览器只在图表显示时将 `revenueFen` 换算为元；`7d` / `30d` 均使用同一项结构：

```json
{
  "ranges": {
    "7d": {
      "range": "7d",
      "items": [{ "date": "2026-09-01", "energyKwh": 128.5, "revenueFen": 93600 }]
    },
    "30d": { "range": "30d", "items": [] }
  }
}
```

这是 V1 的唯一正式 `revenueTrend` 契约：一次 WebSocket `DASHBOARD_UPDATE` 必须同时携带 `7d` 和 `30d`。浏览器范围切换只读取已收到的数据，不会也不能向 Qt Server 请求另一范围。旧的按范围单独 payload 仅作为临时兼容输入，控制台会发出 warning，不能作为 Qt/Web 联调实现依据。

`prediction` 复用 API/ML 的 `stationId`、`horizon`、`predictedLoad`、`predictedAvailableCount`、`peakLevel` 语义；`stationName` 仅用于展示，可选。V1 canonical 字段为 `items`：

```json
{
  "generatedAt": "2026-09-02T10:00:00Z",
  "items": [{
    "stationId": 1,
    "stationName": "东软软件园充电站",
    "predictionTime": "2026-09-02 13:00:00",
    "horizon": "1h",
    "predictedLoad": 0.62,
    "predictedAvailableCount": 3,
    "peakLevel": "MEDIUM"
  }]
}
```

上述数据形状是 Web 第一版的最小前端约定：`summary` 使用固定字段，`pileStatus` 使用 `{ "counts": {} }`，`revenueTrend` 使用 `{ "ranges": { "7d": {}, "30d": {} } }`，`prediction` 使用 `{ "items": [] }`。不修改公共 message type 或已有字段含义；非 canonical 的临时 fallback 仅会被带 warning 地处理。

## Qt 服务端联调 TODO

当前默认 Mock 只用于独立开发、演示和测试。真实模式下浏览器仍只通过既有 `/dashboard` WebSocket 接收四类 topic，绝不直接访问数据库。

当前 `DashboardWebSocketServer::publish(topic, data)` 已经完成连接、订阅和广播，但 Qt 业务 Service/Repository 尚未提交。因此以下项目仍未完成：

1. Qt 业务统计层；
2. `summary` 的真实数据聚合；
3. `pileStatus` 的真实数据聚合；
4. `revenueTrend` 7d + 30d 的真实统计，并在一次 update 中提供两段数据；
5. `prediction` 的真实数据来源；
6. Qt 业务变化后的 `publish(topic, data)`；
7. WebSocket subscribe 后 initial snapshot；
8. 最终 Qt/Web 联调；
9. prediction 正式时间序列展示形式：如果真实 ML 对同一站点、同一 horizon 返回多个 `predictionTime`，图表 X 轴可能应改为时间轴；当前 V1 保留现有实现，待真实 ML 数据结构联调后再决定。

后续 Qt 侧应在业务动作完成并获得可信统计后，调用既有接口：

```text
Qt business data
  -> dashboard statistics / prediction result
  -> DashboardWebSocketServer::publish("summary" | "pileStatus" | "revenueTrend" | "prediction", data)
  -> Browser
```

浏览器不需要结构性重写：服务端只需按本文档的数据形状推送四个既有 topic。若服务端暂时只在事件发生时推送，浏览器会保留最后一次有效数据；initial snapshot 的服务端实现仍是联调待办。
