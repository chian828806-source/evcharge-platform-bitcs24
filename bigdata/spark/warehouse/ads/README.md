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
## Future Implementation
实现 ADS SQL、血缘和刷新检查。
## Acceptance
每个 API 字段可回溯到 ADS、DWS 和 batch。
