# 22. Raw / ODS 数据契约

## 1. 全局规则

来源语义以 `database/schema.sql` 与 `docs/04-DATABASE.md` 为准。CSV 使用 UTF-8、逗号分隔、
首行为 snake_case header；时间为 `yyyy-MM-dd HH:mm:ss`，业务时间为 `Asia/Shanghai`，
`charging_session_history` 的外部历史时间标为 UTC。金额字段均为整数 `*_fen`（分），电量为
`kwh`，功率为 `kw`，负荷为 0–1 小数；空值以空字符串表示。ODS 路径：
`/evcharge/ods/<dataset>/dt=YYYY-MM-DD/batch=<batch_id>/`。

`batch_id`、`source_file`、`ingested_at` 是摄取元数据，不属于业务字段。使用固定随机种子生成
时必须将 seed 写入 manifest。

## 2. 数据集

| 文件 | 来源一期实体 | 主键/粒度 | 字段（类型；nullable） |
| --- | --- | --- | --- |
| `users.csv` | `user` | `user_id`；用户分析维度 | `user_id:int;N, status:string;N, created_at:timestamp;N` |
| `stations.csv` | `charging_station` | `station_id`；站点 | `station_id:int;N, station_no:string;N, name:string;N, district:string;Y, longitude:double;N, latitude:double;N, price_fen_per_kwh:int;N, service_fee_fen_per_kwh:int;N, status:string;N, created_at:timestamp;N` |
| `piles.csv` | `charging_pile` | `pile_id`；电桩 | `pile_id:int;N, station_id:int;N, pile_no:string;N, type:string;N, power_kw:double;N, status:string;N, total_charge_count:int;N, total_charge_minutes:int;N, total_energy_kwh:double;N, updated_at:timestamp;N` |
| `orders.csv` | `charging_order` | `order_id`；订单 | `order_id:int;N, order_no:string;N, user_id:int;N, station_id:int;N, pile_id:int;N, status:string;N, start_at:timestamp;Y, end_at:timestamp;Y, charge_minutes:int;N, energy_kwh:double;N, amount_fen:int;N, paid_at:timestamp;Y, created_at:timestamp;N` |
| `sessions.csv` | `charging_session_history` | `source_session_key`；外部会话 | `source_session_key:string;N, station_id:int;N, start_at:timestamp;N, end_at:timestamp;N, duration_seconds:int;N, energy_kwh:double;N, source_name:string;N` |
| `station_hourly_metrics.csv` | `station_hourly_metric` | `(station_id,hour_start)`；站点小时 | `station_id:int;N, hour_start:timestamp;N, total_pile_count:int;N, session_starts:int;N, energy_kwh:double;N, charging_pile_minutes:double;N, average_occupied_count:double;N, average_available_count:double;N, station_load:double;N, source_type:string;N` |

`users.csv` 仅可包含上述最小分析字段；不得导出 phone、nickname、avatar、密码或钱包余额。
`status` 的合法值与一期数据库一致：用户 `NORMAL/FROZEN`，站点 `NORMAL/DISABLED`，电桩
`AVAILABLE/RESERVED/CHARGING/FAULT/OFFLINE/RESTARTING`，订单
`CREATED/CHARGING/PENDING_PAYMENT/COMPLETED/CANCELLED`，电桩类型 `FAST/SLOW`。

## 3. 质量规则与注入

| 规则 | 作用数据 | 不合格条件 | 处置 |
| --- | --- | --- | --- |
| DQ-001 | 全部 | 主键为空或重复 | 拒绝重复/空键记录 |
| DQ-002 | 必填字段 | 必填值为空、类型无法解析 | 拒绝并记录列名 |
| DQ-003 | 数值 | kWh、金额、时长为负；功率非正；负荷不在 0–1 | 拒绝 |
| DQ-004 | 时间 | `end_at <= start_at`、`paid_at < start_at` | 拒绝 |
| DQ-005 | 枚举 | 不在上述状态/类型集合 | 拒绝 |
| DQ-006 | 关联 | 订单/会话引用不存在站点或电桩 | 拒绝 |

生成器至少以约 1%–3% 的比例、可配置且可复现地注入上述六类问题；正常记录不得被刻意改写。
质量报告必须同时给出原始行数、各规则数、有效行数、拒绝行数与样例。
