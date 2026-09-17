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
## Run

```bash
spark-submit bigdata/spark/warehouse/build_warehouse.py \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-DEMO \
  --replace
```

作业只读取同批次的四张 DWD 表，使用 Spark SQL 发布四张 DWS、六张 ADS、批次 manifest，
同时原子更新 Flask 使用的 `bigdata/runtime/warehouse/dashboard.json`。营收和业务电量仅统计
`COMPLETED + paid_at` 的真实订单，排除 `URBANEV-SYN-%` 关系补全记录。
## Acceptance
每张输出表有粒度、主键、分区、批次和下游用途。
