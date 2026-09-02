# Web Dashboard

`web-dashboard/` 是 EVCharge 的浏览器运营展示模块。它只负责将服务端 WebSocket 推送转换为页面指标、表格和 ECharts 数据；不访问 SQLite、不包含 Qt 业务逻辑，也不重新实现服务端。

## 当前完成内容

- `DashboardWebSocketClient`：连接 `/dashboard`、发送 `DASHBOARD_SUBSCRIBE`、处理 `DASHBOARD_UPDATE`、安全忽略非法消息、指数退避重连并在重连后自动订阅。
- `DashboardStore`：集中保存四个 topic、连接状态、最后更新时间和最后一次有效数据。断线不会清空页面数据。
- `DashboardController`：将 Store 数据绑定到指标、筛选器、表格和交互入口。
- `DashboardCharts`：每个 ECharts 实例只初始化一次，后续只调用 `setOption()`；空数据、缺失字段和 resize 都安全处理。
- 可关闭 Mock 模式：所有演示数据集中在 `mock/dashboard-mock.js`，不散落在图表或页面代码中。
- `tests/dashboard-logic-tests.html`：不依赖前端框架的浏览器测试页，覆盖 Store、订阅、消息路由和 Mock 四 topic。

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

`summary` 的前三个字段来自既有 WebSocket 示例；`totalOrderCount` 是可选展示字段，缺失时显示 `—`：

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

`revenueTrend` 以分保存金额，浏览器仅在显示时换算元：

```json
{
  "days7": [{ "date": "09-02", "revenueFen": 93600 }],
  "days30": []
}
```

`prediction` 复用 API/ML 的 `stationId`、`horizon`、`predictedLoad`、`predictedAvailableCount`、`peakLevel` 语义；`stationName` 仅用于展示，可选：

```json
{
  "generatedAt": "2026-09-02T10:00:00Z",
  "predictions": [{
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

这三种除 `summary` 外的数据形状是 Web 第一版的最小前端约定，不修改公共 message type 或已有字段含义；Qt 业务联调时需确认最终统计查询与字段是否还需补充。

## Qt 服务端联调 TODO

当前 `DashboardWebSocketServer::publish(topic, data)` 已经完成连接、订阅和广播，但 Qt 业务 Service/Repository 尚未提交。因此目前没有：

1. 业务统计聚合；
2. 订阅后的 initial snapshot；
3. 订单、充电桩、预测变化触发的 publish 调用。

后续 Qt 侧应在业务动作完成并获得可信统计后，调用既有接口：

```text
Qt business data
  -> dashboard statistics / prediction result
  -> DashboardWebSocketServer::publish("summary" | "pileStatus" | "revenueTrend" | "prediction", data)
  -> Browser
```

浏览器不需要结构性重写：服务端只需按本文档的数据形状推送四个既有 topic。若服务端暂时只在事件发生时推送，浏览器会保留最后一次有效数据；initial snapshot 的服务端实现仍是联调待办。
