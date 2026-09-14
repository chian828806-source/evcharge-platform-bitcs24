# 24. Dashboard REST API 契约

Base URL：`/api/v1`。所有接口为 `GET`、返回 JSON、面向分析数据；不提供一期订单写操作。
共同 envelope 为 `{"data": ..., "meta":{"batchId":"...","generatedAt":"..."}}`。成功空数据返回
`200` 与空数组/零值；非法 query 返回 `400`，不存在资源 `404`，未完成批次 `409`，服务异常 `500`，
错误格式为 `{"error":{"code":"...","message":"..."}}`。

时间 query 使用 `from`、`to`（`YYYY-MM-DD`），站点可选 `stationId`；无 query 时由服务规定默认
近 30 日，并在 `meta` 回显范围。金额字段一律 `revenueFen`，展示层自行除以 100。

| Endpoint / ADS | 响应 `data` 字段（单位） | 空数据示例 |
| --- | --- | --- |
| `/dashboard/overview` / overview | `orderCount:int, energyKwh:double, revenueFen:int, onlinePileCount:int, utilizationRate:double` | `{ "orderCount":0,"energyKwh":0,"revenueFen":0,"onlinePileCount":0,"utilizationRate":0 }` |
| `/dashboard/energy-trend` / energy_trend | `items:[{date,energyKwh,orderCount}]` | `{ "items":[] }` |
| `/dashboard/revenue-trend` / revenue_trend | `items:[{date,revenueFen,orderCount}]` | `{ "items":[] }` |
| `/dashboard/station-ranking` / station_rank | `items:[{stationId,stationName,district,energyKwh,revenueFen,utilizationRate,rank}]` | `{ "items":[] }` |
| `/dashboard/pile-status` / pile_status | `items:[{status,count,ratio}]`；ratio 0–1 | `{ "items":[] }` |
| `/dashboard/hourly-heatmap` / hour_heatmap | `items:[{dayOfWeek,hour,energyKwh,utilizationRate}]` | `{ "items":[] }` |
| `/dashboard/station-utilization` / station_day | `items:[{stationId,stationName,date,utilizationRate,availableCount,totalPileCount}]` | `{ "items":[] }` |
| `/dashboard/prediction` / prediction | `items:[{stationId,stationName,predictionTime,horizon,predictedLoad,predictedAvailableCount,peakLevel,modelName,mae,rmse}]` | `{ "items":[] }` |
| `/data-quality/summary` / quality report | `sourceRows:int,acceptedRows:int,rejectedRows:int,rules:[{ruleId,count}]` | `{ "sourceRows":0,"acceptedRows":0,"rejectedRows":0,"rules":[] }` |

典型成功响应：

```json
{"data":{"items":[{"date":"2026-09-01","energyKwh":123.5,"orderCount":18}]},"meta":{"batchId":"BD-20260901-01","generatedAt":"2026-09-01 08:00:00"}}
```

`peakLevel` 仅允许 `LOW/MEDIUM/HIGH`；`predictedLoad`、`utilizationRate`、`ratio` 为 0–1。Vue
只能按本文件消费 Flask；Qt WebSocket 若接入，仅作为单独标明的实时状态扩展。
