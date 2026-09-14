# DWD contract

## Module Responsibility
定义质量通过后的规范化明细边界。
## Owner
A（Contract）；B 为 DWD 生产者，C 为消费者。
## Input
ODS 与 `DQ-*` 质量结果。
## Output
DWD 明细和拒绝原因。
## Allowed Dependencies
Raw/ODS Contract、质量规则。
## Forbidden Dependencies
C 绕过 DWD 读取 Raw，或静默丢弃拒绝记录。
## Public Contract
[23-WAREHOUSE-DESIGN.md](../../../docs/bigdata/23-WAREHOUSE-DESIGN.md)。
## Future Implementation
增加 DWD schema/主键测试。
## Acceptance
每条拒绝记录有原因，C 能仅以 DWD 建立数仓。
