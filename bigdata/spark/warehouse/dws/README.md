# DWS SQL

## Module Responsibility
构建站点小时/日、区域日和电桩日主题汇总。
## Owner
C（Warehouse / Analytics）。
## Input
DWD。
## Output
稳定 DWS 指标与 ML feature 输入。
## Allowed Dependencies
DWD、Warehouse Design。
## Forbidden Dependencies
Raw、UI 或模型实现细节。
## Public Contract
`dws_station_hour` 等定义见 23 号文档。
## Future Implementation
实现按批次/日期的聚合 SQL。
## Acceptance
E 可只依赖 DWS 训练，不需要读取 Raw。
