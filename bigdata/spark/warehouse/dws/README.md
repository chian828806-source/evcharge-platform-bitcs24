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
## Implementation
`../build_warehouse.py` 发布 `dws_station_hour`、`dws_station_day`、`dws_region_day` 和
`dws_pile_day`。所有表均携带 `batch_id`、`calculated_at` 和日期字段；站点小时表保持 E 所需的
ML Feature Contract，并增加正式营收口径字段。
## Acceptance
E 可只依赖 DWS 训练，不需要读取 Raw。
