# Ingestion

## Module Responsibility
将 Raw CSV 以批次和日期分区摄取到 HDFS ODS。
## Owner
B（Data Pipeline）。
## Input
Raw CSV、manifest 和 HDFS 环境。
## Output
`/evcharge/ods/<dataset>/dt=<date>/batch=<id>` 与摄取清单。
## Allowed Dependencies
Raw/ODS Contract、environment 输出。
## Forbidden Dependencies
修改 Raw 值、直接写 DWS/ADS 或伪造成功标记。
## Public Contract
批次号、哈希、行数、路径和摄取时间。
## Run

在 Hadoop 虚拟机执行：

```bash
python3 bigdata/ingestion/ingest_ods.py \
  --raw-dir output/raw/BD-20260915-001 \
  --business-date 2026-09-15 \
  --batch-id BD-20260915-001
```

默认拒绝覆盖已存在批次。只有明确确认需要重传同一批次时，才增加 `--replace`；该选项仅删除该批次
对应的 HDFS ODS 路径。
## Acceptance
文件可列举/读取，失败不发布 success manifest。
