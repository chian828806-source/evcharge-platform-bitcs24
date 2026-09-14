# Spark MLlib

## Module Responsibility
使用冻结 DWS 特征完成 MLlib 训练、评价和 `1h/6h/24h` 预测。
## Owner
E（Spark MLlib）。
## Input
C 提供的 DWS / ML Feature Contract。
## Output
模型元数据、评价报告和 `ads_prediction`。
## Allowed Dependencies
DWS、ML Contract、Spark MLlib。
## Forbidden Dependencies
Raw、一期 SQLite、Vue、Flask/Core 内部实现。
## Public Contract
[25-ML-CONTRACT.md](../../docs/bigdata/25-ML-CONTRACT.md)。
## Future Implementation
实现特征工程、时间切分、baseline 对比和批量发布。
## Acceptance
预测范围、指标和版本齐全，C 可查询 ads_prediction 而无需读取模型文件。
