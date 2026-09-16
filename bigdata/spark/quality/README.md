# Data quality

## Module Responsibility
以 PySpark 检查 ODS 的模式、空值、重复、范围、时间、枚举和关联。
## Owner
B（Data Pipeline / Data Quality）。
## Input
ODS 与 `DQ-*` 规则。
## Output
质量摘要、可展示报告、rejected 记录和原因。
## Allowed Dependencies
Raw/ODS Contract 和 PySpark 环境。
## Forbidden Dependencies
修改 Generator Contract、自行实现 DWS/ADS/API/ML。
## Public Contract
DQ 报告与 DWD Contract。
## Run

在 Hadoop 虚拟机执行：

```bash
spark-submit bigdata/spark/quality/quality_check.py \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

作业读取 ODS，发布 accepted Parquet、带 DQ 原因的 rejected Parquet 以及批次质量报告。默认拒绝
覆盖同一质量输出批次；重跑该批次时才增加 `--replace`。
## Acceptance
预设异常均被发现并能统计、定位和追溯。
