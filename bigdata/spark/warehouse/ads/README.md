# ADS SQL

## Module Responsibility
构建直接支撑 API、Dashboard Core 和预测查询的应用数据集。
## Owner
C（Warehouse / Analytics）。
## Input
DWS 与 E 发布的 `ads_prediction`。
## Output
版本化 ADS 指标和批次元数据。
## Allowed Dependencies
DWS/ADS/ML Contract。
## Forbidden Dependencies
Vue UI、SQLite 业务写入或模型训练内部实现。
## Public Contract
ADS 字段对应 23 号文档和 24 号 HTTP Contract。
## Implementation
`../build_warehouse.py` 发布 overview、站点排行、能耗趋势、营收趋势、桩状态和小时热力六类
ADS。E 的 `ads_prediction` 由 Flask 按相同 `batchId` 合并，批次不一致时不会混用。
## Acceptance
每个 API 字段可回溯到 ADS、DWS 和 batch。
