# DWD transformations

## Module Responsibility
清洗、类型统一、去重、异常隔离并发布 DWD。
## Owner
B（Data Pipeline / Data Quality）。
## Input
ODS、质量结果与 DWD Contract。
## Output
合法 DWD 明细及带原因的 rejected 输出。
## Allowed Dependencies
PySpark、Raw/ODS/DWD Contract。
## Forbidden Dependencies
改写 Raw、放宽质量契约、直接实现 DWS/ADS/API/ML。
## Public Contract
[contracts/dwd/README.md](../../contracts/dwd/README.md)。
字段设计提案见 [DWD-SCHEMA-DRAFT.md](DWD-SCHEMA-DRAFT.md)；该文件在 A、C 评审前不视为正式
公共 Contract。
## Future Implementation
实现 DWD 表、清洗和拒绝隔离。

## Run

在 Hadoop 虚拟机执行：

```bash
spark-submit bigdata/spark/dwd/build_dwd.py \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

作业只读取 B 发布的 quality accepted 数据，输出四张 Parquet DWD 表；默认拒绝覆盖已有输出。
## Acceptance
C 可只读取 DWD；非支付订单也按 `created_at` 分区保留。
