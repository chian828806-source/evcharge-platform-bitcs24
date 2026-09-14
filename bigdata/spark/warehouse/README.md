# Spark SQL warehouse

## Module Responsibility
从 DWD 建设 DWS/ADS、运营 KPI 和可追溯 SparkSQL 分析。
## Owner
C（Warehouse / Analytics / Flask API）。
## Input
B 输出的 DWD Contract。
## Output
DWS、ADS、血缘、批次和指标口径。
## Allowed Dependencies
DWD Contract、SparkSQL、Warehouse Design。
## Forbidden Dependencies
Raw 重清洗、Vue/ECharts、ML 训练或绕过 DWD。
## Public Contract
[23-WAREHOUSE-DESIGN.md](../../../docs/bigdata/23-WAREHOUSE-DESIGN.md)。
## Future Implementation
补充各层建表、ETL 与指标 SQL。
## Acceptance
每张输出表有粒度、主键、分区、批次和下游用途。
