# DWD 字段设计草案

状态：待 A、B、C 共同评审，尚未成为正式公共 Contract

生产者：B（Data Pipeline / Data Quality）

消费者：C（Warehouse / Analytics）

依据：`docs/bigdata/22-DATA-CONTRACT.md`、`docs/bigdata/23-WAREHOUSE-DESIGN.md`

## 1. 设计原则

1. DWD 只保存通过 DQ-001 至 DQ-006 的合法记录；不合格记录进入 rejected 数据集，不能静默丢弃。
2. 业务字段保持 snake_case；金额统一使用 `BIGINT`，单位为分；电量、功率和比例使用 `DOUBLE`。
3. 一期业务时间按 `Asia/Shanghai` 解释；外部充电会话的 `start_at`、`end_at` 按 UTC 解释。
4. `batch_id`、`source_file`、`ingested_at`、`processed_at` 用于数据追溯，不属于一期业务字段。
5. `dt` 是物理分区字段。订单取 `created_at` 的日期，会话取 `start_at` 的 UTC 日期，电桩取 `updated_at` 的日期，小时指标取 `hour_start` 的日期。
6. 订单、电桩、会话和小时指标从质量通过的站点、电桩数据中取得必要的维度快照，确保 C 无需绕过 DWD 读取 ODS。
7. 站点或电桩名称等快照字段只用于历史展示和分组，不能替代对应 ID 作为关联键。

## 2. 公共审计字段

四张 DWD 表均包含以下字段：

| 字段 | Spark SQL 类型 | 可空 | 来源或规则 |
| --- | --- | --- | --- |
| `batch_id` | STRING | 否 | 摄取批次号 |
| `source_file` | STRING | 否 | 原始 CSV 文件名 |
| `ingested_at` | TIMESTAMP | 否 | ODS 摄取时间 |
| `processed_at` | TIMESTAMP | 否 | DWD 作业处理时间 |
| `dt` | DATE | 否 | 表对应业务时间的日期；物理分区字段 |

## 3. dwd_order_detail

粒度：每个合法业务订单一行。

主键：`order_id`；同时校验 `order_no` 唯一。

来源：`ods_order`，并关联 `ods_station`、`ods_pile` 做存在性校验和维度快照。

分区：`dt = to_date(created_at)`，时区为 `Asia/Shanghai`。

| 字段 | Spark SQL 类型 | 可空 | 来源或规则 |
| --- | --- | --- | --- |
| `order_id` | BIGINT | 否 | `orders.order_id` |
| `order_no` | STRING | 否 | `orders.order_no` |
| `user_id` | BIGINT | 否 | `orders.user_id` |
| `station_id` | BIGINT | 否 | `orders.station_id`；必须关联存在的站点 |
| `station_no` | STRING | 否 | 关联 `stations.station_no` 的批次快照 |
| `station_name` | STRING | 否 | 关联 `stations.name` 的批次快照 |
| `district` | STRING | 是 | 关联 `stations.district` 的批次快照 |
| `station_status` | STRING | 否 | 关联 `stations.status` 的批次快照 |
| `pile_id` | BIGINT | 否 | `orders.pile_id`；必须属于同一 `station_id` |
| `pile_no` | STRING | 否 | 关联 `piles.pile_no` 的批次快照 |
| `pile_type` | STRING | 否 | 关联 `piles.type`，仅 `FAST/SLOW` |
| `pile_power_kw` | DOUBLE | 否 | 关联 `piles.power_kw`，必须大于 0 |
| `status` | STRING | 否 | 订单五态之一 |
| `start_at` | TIMESTAMP | 是 | 开始充电时间，`Asia/Shanghai` |
| `end_at` | TIMESTAMP | 是 | 停止充电时间，`Asia/Shanghai` |
| `charge_minutes` | INT | 否 | 必须大于等于 0 |
| `energy_kwh` | DOUBLE | 否 | 必须大于等于 0 |
| `amount_fen` | BIGINT | 否 | 订单应付金额，必须大于等于 0 |
| `paid_at` | TIMESTAMP | 是 | 支付时间；营收只认 `COMPLETED` 且非空 |
| `created_at` | TIMESTAMP | 否 | 订单创建时间，`Asia/Shanghai` |

说明：当前 Raw Contract 没有下单时的电价、服务费、优惠券字段，因此本版 DWD 不虚构这些字段。若后续需要分析折扣或服务费，须先通过 CCR 扩展 `orders.csv` Contract。

## 4. dwd_charging_session_detail

粒度：每个合法外部充电会话一行。

主键：`source_session_key`。

来源：`ods_session`，并关联 `ods_station` 做存在性校验和维度快照。

分区：`dt = to_date(start_at)`，这里的日期按 UTC 解释。

| 字段 | Spark SQL 类型 | 可空 | 来源或规则 |
| --- | --- | --- | --- |
| `source_session_key` | STRING | 否 | `sessions.source_session_key` |
| `station_id` | BIGINT | 否 | `sessions.station_id`；必须关联存在的站点 |
| `station_no` | STRING | 否 | 关联 `stations.station_no` 的批次快照 |
| `station_name` | STRING | 否 | 关联 `stations.name` 的批次快照 |
| `district` | STRING | 是 | 关联 `stations.district` 的批次快照 |
| `station_status` | STRING | 否 | 关联 `stations.status` 的批次快照 |
| `source_station_name` | STRING | 是 | 外部数据原始站点名称，保留用于追溯 |
| `start_at` | TIMESTAMP | 否 | 会话开始时间，UTC |
| `end_at` | TIMESTAMP | 否 | 会话结束时间，UTC；必须晚于 `start_at` |
| `duration_seconds` | BIGINT | 否 | 必须大于 0，并校验与起止时间基本一致 |
| `energy_kwh` | DOUBLE | 否 | 必须大于等于 0 |

说明：外部会话不能混入一期业务营收，只能用于历史负荷、会话趋势和模型分析。

## 5. dwd_pile_detail

粒度：每个批次、每个电桩的一次状态快照。

业务主键：`pile_id + dt`；每个发布批次内唯一。如果同一批次同一天重复，以 `updated_at` 最新且质量通过的记录为准。

来源：`ods_pile`，并关联 `ods_station` 做存在性校验和维度快照。

分区：`dt = to_date(updated_at)`，时区为 `Asia/Shanghai`。

| 字段 | Spark SQL 类型 | 可空 | 来源或规则 |
| --- | --- | --- | --- |
| `pile_id` | BIGINT | 否 | `piles.pile_id` |
| `station_id` | BIGINT | 否 | `piles.station_id`；必须关联存在的站点 |
| `station_no` | STRING | 否 | 关联 `stations.station_no` 的批次快照 |
| `station_name` | STRING | 否 | 关联 `stations.name` 的批次快照 |
| `district` | STRING | 是 | 关联 `stations.district` 的批次快照 |
| `station_status` | STRING | 否 | 关联 `stations.status` 的批次快照 |
| `longitude` | DOUBLE | 否 | 关联 `stations.longitude`，GCJ-02 |
| `latitude` | DOUBLE | 否 | 关联 `stations.latitude`，GCJ-02 |
| `pile_no` | STRING | 否 | `piles.pile_no`；站内唯一 |
| `pile_type` | STRING | 否 | `piles.type`，仅 `FAST/SLOW` |
| `power_kw` | DOUBLE | 否 | 额定功率，必须大于 0 |
| `status` | STRING | 否 | 电桩六态之一 |
| `total_charge_count` | BIGINT | 否 | 累计完成充电次数，必须大于等于 0 |
| `total_charge_minutes` | BIGINT | 否 | 累计充电分钟数，必须大于等于 0 |
| `total_energy_kwh` | DOUBLE | 否 | 累计充电量，必须大于等于 0 |
| `updated_at` | TIMESTAMP | 否 | 快照业务时间，`Asia/Shanghai` |

说明：该表是批次快照，不表示完整的实时状态流水。Dashboard 如需实时状态，应由 Qt WebSocket adapter 提供。

## 6. dwd_station_hour_metric

粒度：每个站点每个自然小时一行。

主键：`station_id + hour_start`；重跑时以同一 `batch_id` 幂等覆盖。

来源：`ods_hourly_metric`，并关联 `ods_station` 做存在性校验和维度快照。

分区：`dt = to_date(hour_start)`，时区为 `Asia/Shanghai`。

| 字段 | Spark SQL 类型 | 可空 | 来源或规则 |
| --- | --- | --- | --- |
| `station_id` | BIGINT | 否 | `station_hourly_metrics.station_id` |
| `station_no` | STRING | 否 | 关联 `stations.station_no` 的批次快照 |
| `station_name` | STRING | 否 | 关联 `stations.name` 的批次快照 |
| `district` | STRING | 是 | 关联 `stations.district` 的批次快照 |
| `station_status` | STRING | 否 | 关联 `stations.status` 的批次快照 |
| `hour_start` | TIMESTAMP | 否 | 小时起点，必须对齐整点 |
| `total_pile_count` | INT | 否 | 必须大于 0 |
| `session_starts` | INT | 否 | 本小时开始会话数，必须大于等于 0 |
| `energy_kwh` | DOUBLE | 否 | 本小时充电量，必须大于等于 0 |
| `charging_pile_minutes` | DOUBLE | 否 | 本小时充电桩分钟数，范围为 `0..total_pile_count*60` |
| `average_occupied_count` | DOUBLE | 否 | 范围为 `0..total_pile_count` |
| `average_available_count` | DOUBLE | 否 | 范围为 `0..total_pile_count` |
| `station_load` | DOUBLE | 否 | 范围为 `0..1` |
| `source_type` | STRING | 否 | 仅 `BUSINESS/CARY_SIMULATION` |

说明：DWD 保留清洗后的小时事实，不在此层计算日汇总、排行或 Dashboard KPI。C 在 DWS 中按正式口径计算 `utilization_rate = charging_pile_minutes / (total_pile_count * 60)`。

## 7. rejected 输出的最小字段

rejected 不是上述四张业务表之一，但属于 B 的必交付结果。建议所有数据集使用统一结构：

| 字段 | Spark SQL 类型 | 说明 |
| --- | --- | --- |
| `dataset` | STRING | 原始数据集名称 |
| `batch_id` | STRING | 摄取批次号 |
| `source_file` | STRING | 来源文件 |
| `raw_record` | STRING | 原始记录 JSON，避免异常值在强转时丢失 |
| `rule_ids` | ARRAY&lt;STRING&gt; | 命中的 DQ 规则，可同时命中多条 |
| `reasons` | ARRAY&lt;STRING&gt; | 可展示的具体拒绝原因 |
| `rejected_at` | TIMESTAMP | 拒绝处理时间 |
| `dt` | DATE | 拒绝记录分区日期 |

## 8. 需要 A、C 确认的 Contract 变更

以下内容超出了当前 23 号文档的概要定义，正式实现前需要评审：

1. 四张表均加入公共审计字段和 `batch_id`。
2. `dwd_charging_session_detail`、`dwd_station_hour_metric` 的来源增加 `ods_station`，用于关联校验和站点维度快照。
3. DWD 记录均携带 `batch_id`；业务主键在单个发布批次内判重，下游按已发布批次读取，不能混合多个批次重复统计。
4. 当前 Raw 订单缺少价格、服务费和优惠券快照；本版 DWD 不提供相关分析字段。
5. C 确认上述字段能覆盖 DWS 站点小时、站点日、区域日和电桩日的计算需求。

评审通过后，由 A 将最终字段同步到正式 DWD Contract，并增加 Schema 自动校验；B 再据此实现 PySpark 清洗和写表。
