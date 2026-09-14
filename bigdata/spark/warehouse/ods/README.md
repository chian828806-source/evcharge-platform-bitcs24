# ODS SQL

## Module Responsibility
注册 Raw 文件、分区和来源元数据。
## Owner
C（Warehouse），B 负责写入 HDFS。
## Input
B 摄取的 ODS 路径与 manifest。
## Output
可查询 ODS 表定义。
## Allowed Dependencies
Raw/ODS Contract。
## Forbidden Dependencies
业务清洗、DWD/DWS/ADS 指标重算。
## Public Contract
22 号文档的 dataset、batch 与分区规则。
## Future Implementation
添加外部表/格式定义。
## Acceptance
可按 batch 和 dt 查询，原始值不被修改。
