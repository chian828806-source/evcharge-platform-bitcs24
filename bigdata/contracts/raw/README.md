# Raw / ODS contract

## Module Responsibility
冻结原始文件、摄取元数据和 ODS 不变性。
## Owner
A（Contract）；B 为生产者。
## Input
一期 Schema 与 B 生成/摄取的 CSV。
## Output
可读的 Raw/ODS Contract。
## Allowed Dependencies
`22-DATA-CONTRACT.md`。
## Forbidden Dependencies
在 ODS 修正业务值或让下游依赖生成器内部代码。
## Public Contract
[22-DATA-CONTRACT.md](../../../docs/bigdata/22-DATA-CONTRACT.md)。
## Future Implementation
增加 schema validator 和 manifest 示例。
## Acceptance
文件名、字段、编码、时区、分区与质量注入可被独立验证。
