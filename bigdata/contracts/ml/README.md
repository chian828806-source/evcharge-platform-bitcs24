# ML contract

## Module Responsibility
定义 ML 特征、评价和预测发布边界。
## Owner
A（Contract）；E 为生产者，C/A 为消费者。
## Input
冻结的 DWS ML feature。
## Output
`ads_prediction` 与模型元数据。
## Allowed Dependencies
DWS/ML Contract。
## Forbidden Dependencies
Raw、一期 SQLite、Vue 或模型文件直连。
## Public Contract
[25-ML-CONTRACT.md](../../../docs/bigdata/25-ML-CONTRACT.md)。
## Future Implementation
增加预测 schema 和指标 gate。
## Acceptance
仅允许 `1h/6h/24h` 合法预测进入 API 发布路径。
