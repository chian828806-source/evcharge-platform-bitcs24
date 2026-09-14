# DWS / ADS contract

## Module Responsibility
定义主题汇总、应用指标、批次和业务口径。
## Owner
A（Contract）；C 为生产者，E/A 为消费者。
## Input
B 输出的 DWD。
## Output
DWS、ADS 和可追溯 `ads_prediction` 查询边界。
## Allowed Dependencies
DWD Contract、指标口径。
## Forbidden Dependencies
消费者引用 warehouse 内部实现或另算正式 KPI。
## Public Contract
[23-WAREHOUSE-DESIGN.md](../../../docs/bigdata/23-WAREHOUSE-DESIGN.md)。
## Future Implementation
增加表级血缘和批次校验。
## Acceptance
Flask、ML 与 Core 可独立消费定义的字段和单位。
