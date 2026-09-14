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
## Future Implementation
实现幂等摄取和失败恢复。
## Acceptance
文件可列举/读取，失败不发布 success manifest。
