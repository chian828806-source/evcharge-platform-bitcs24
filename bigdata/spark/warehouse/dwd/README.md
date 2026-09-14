# DWD SQL

## Module Responsibility
登记 B 发布的规范化 DWD 明细供数仓消费。
## Owner
C（Warehouse）；B 是 DWD 规则/数据 Owner。
## Input
B 的 DWD 输出和 DWD Contract。
## Output
可查询的 DWD 表引用。
## Allowed Dependencies
DWD Contract、SparkSQL。
## Forbidden Dependencies
修改清洗规则、绕过 B 读取 Raw。
## Public Contract
`dwd_order_detail` 等表的主键与分区以 23 号文档为准。
## Future Implementation
添加表注册和 schema 校验。
## Acceptance
不改变 B 的 DWD 语义，C 可据此生成 DWS。
