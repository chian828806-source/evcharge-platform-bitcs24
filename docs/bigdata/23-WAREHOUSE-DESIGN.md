# 23. ODS-DWD-DWS-ADS 数仓设计

## 1. 分层原则

ODS 保留原始契约和摄取批次；DWD 是通过 DQ 规则的规范化明细；DWS 是可复用主题汇总；ADS 是
直接供 API、大屏和 ML 消费的应用数据。所有业务金额保持 `fen`，API 再按需转换展示。

| 层/表 | 来源 | 粒度、主键与分区 | 用途/下游 |
| --- | --- | --- | --- |
| `ods_user/station/pile/order/session/hourly_metric` | 22 号文档 CSV | 原文件记录；`dt,batch_id` | 可追溯原始层 |
| `dwd_order_detail` | ods_order + station + pile | 每合法订单；`order_id`，`dt=created_at` | 全量订单明细，供后续状态/营收分析 |
| `dwd_charging_session_detail` | ods_session | 每有效会话；`source_session_key`，`dt=start_at` | 外部历史事实 |
| `dwd_pile_detail` | ods_pile + station | 每电桩快照；`pile_id,dt` | 状态与容量维度 |
| `dwd_station_hour_metric` | ods_hourly_metric | 每站每小时；`station_id,hour_start`，`dt` | 负荷主题事实 |
| `dws_station_hour` | dwd order/hour metric | 每站每小时；`station_id,hour_start`，`dt` | 预测与热力 |
| `dws_station_day` | dws_station_hour | 每站每天；`station_id,stat_date` | 趋势、利用率、排行 |
| `dws_region_day` | dws_station_day + station | 每区每天；`district,stat_date` | 区域对比 |
| `dws_pile_day` | dwd pile/order | 每桩每天；`pile_id,stat_date` | 设备利用率 |
| `ads_dashboard_overview` | dws station/day + pile | 每批次、日期范围；`batch_id` | KPI |
| `ads_station_rank` | dws_station_day | 每站、窗口；`station_id,window_end` | TOP 排行 |
| `ads_energy_trend/revenue_trend` | dws_station_day | 每天、可选站点；`stat_date,station_id` | 折线图 |
| `ads_pile_status` | dwd_pile_detail | 状态、快照日；`status,stat_date` | 状态环图 |
| `ads_hour_heatmap` | dws_station_hour | 区域/站点、星期、小时；`scope,day_of_week,hour` | 热力图 |
| `ads_prediction` | ML 输出 + station | 站点、目标时刻、预测窗口；`station_id,prediction_time,horizon,model_version` | 预测图 |

## 2. 指标口径

- `revenue_fen`：DWS/ADS 从 DWD 筛选 `status = COMPLETED` 且 `paid_at` 非空，再按 `paid_at`
  的业务日期聚合 `amount_fen`。DWD 保留 CREATED、CHARGING、PENDING_PAYMENT、CANCELLED 等
  合法订单，故不按可空 `paid_at` 分区。
- `energy_kwh`：营收口径中的电量与 `revenue_fen` 使用同一已支付订单筛选；外部会话只能用于
  历史负荷/模型分析，不能混入业务营收。
- `utilization_rate`：`charging_pile_minutes / (total_pile_count × window_minutes)`，范围 0–1；
  只包含 CHARGING，不含 RESERVED/FAULT/OFFLINE/RESTARTING。
- `available_count`：`AVAILABLE` 电桩数；`online_count` 不含 `OFFLINE`。
- 所有 ADS 记录须带 `batch_id` 和计算时间，以便重跑和回滚。

## 3. 血缘与质量门槛

只有 DQ 通过且关联完整的 DWD 才能进入 DWS/ADS。若一个批次存在拒绝记录，报告仍可发布，
但每个 ADS/API 响应必须能标识使用的 `batchId` 和质量摘要版本；禁止把人工修补 JSON 当成 ADS。
