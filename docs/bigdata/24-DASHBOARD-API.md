# 24. Dashboard REST API 契约

Base URL：`/api/v1`。所有接口为 `GET`、返回 JSON、面向分析数据；不提供一期订单写操作。
共同 envelope 为 `{"data": ..., "meta":{"batchId":"...","generatedAt":"..."}}`。成功空数据返回
`200` 与空数组/零值；非法 query 返回 `400`，不存在资源 `404`，未完成批次 `409`，服务异常 `500`，
错误格式为 `{"error":{"code":"...","message":"..."}}`。

时间 query 使用 `from`、`to`（`YYYY-MM-DD`），站点可选 `stationId`；无 query 时由服务规定默认
近 30 日，并在 `meta` 回显范围。金额字段一律 `revenueFen`，展示层自行除以 100。

**Owner 是 C（Flask API），直接 Consumer 是 A（Dashboard Core）。** D（Dashboard UI）不得直接
调用 Flask；它只消费 A 提供的公开 Store / ViewModel。数据链固定为 `ADS → C Flask → A API
Client → A Adapter / Store → Public UI Contract → D Vue UI`。Qt WebSocket 可由 A 的 realtime
adapter 合入同一 Store，但不改变历史 ADS 的 HTTP 口径。

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
| `/context/weather` / Open-Meteo | `city,temperature,apparentTemperature,humidity,precipitation,windSpeed,weatherCode,weatherText,updatedAt,available,isStale,source` | `{ "city":"深圳","temperature":null,"apparentTemperature":null,"humidity":null,"precipitation":null,"windSpeed":null,"weatherCode":null,"weatherText":"暂不可用","updatedAt":null,"available":false,"isStale":true,"source":"unavailable" }` |

典型成功响应：

```json
{"data":{"items":[{"date":"2026-09-01","energyKwh":123.5,"orderCount":18}]},"meta":{"batchId":"BD-20260901-01","generatedAt":"2026-09-01 08:00:00"}}
```

## 实时天气上下文

`GET /context/weather` 是实时辅助信息，不属于 Spark 批次 ADS，因此它的 `meta` 不带
`batchId`。Flask 固定访问 Open-Meteo 的深圳坐标，浏览器不得直连上游。正常响应示例：

```json
{
  "data": {
    "city": "深圳",
    "temperature": 28.4,
    "apparentTemperature": 31.2,
    "humidity": 76,
    "precipitation": 0.0,
    "windSpeed": 12.3,
    "weatherCode": 2,
    "weatherText": "多云",
    "updatedAt": "2026-09-16T18:30:00+08:00",
    "available": true,
    "isStale": false,
    "source": "open-meteo"
  },
  "meta": { "generatedAt": "...", "source": "open-meteo", "cacheTtlSeconds": 1200 }
}
```

- 服务端默认缓存 20 分钟，前端频繁刷新不会等比例请求 Open-Meteo。
- 上游失败且已有缓存时返回旧值，并标记 `isStale:true, source:"cache"`。
- 上游失败且没有缓存时返回表中的 null 降级对象与 HTTP 200，保证大屏其余模块可用；不得伪造温度。
- WMO 天气码由 Flask 统一翻译为中文，Vue 只负责展示。
- 当前接口仅用于大屏实时上下文。ML 若要使用天气特征，必须另建按小时落入 ODS/DWD 的历史天气链路，
  按预测时点切分，禁止把当前天气回填到历史样本。

`peakLevel` 仅允许 `LOW/MEDIUM/HIGH`；`predictedLoad`、`utilizationRate`、`ratio` 为 0–1。Vue
不直接消费本 HTTP Contract。A 必须将 API / Mock / WebSocket 转换为稳定的 Public UI Contract：
`DashboardViewModel = { overview, energyTrend, revenueTrend, stationRanking, pileStatus,
hourlyHeatmap, stationUtilization, prediction, dataQuality, weather, source, loading, error }`。各 collection
沿用上表字段；`source` 为 `mock|api|realtime`，`loading` 为布尔值，`error` 为可展示错误或 null。
空数组、加载和错误均由 Core 正常表达，D 不负责重新取数或重算 KPI。
