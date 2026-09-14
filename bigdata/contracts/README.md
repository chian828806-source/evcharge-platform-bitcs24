# Public contracts

## Module Responsibility
维护 Raw/ODS、DWD、DWS/ADS、ML 与 Dashboard UI 的公共边界。
## Owner
A（Architecture / Contract）。
## Input
经 Owner 提交的 CCR 和一期 Schema 语义。
## Output
版本化 Contract 与兼容性说明。
## Allowed Dependencies
`docs/bigdata/` 与各模块公开输出。
## Forbidden Dependencies
任何模块内部代码、临时文件或未审批字段。
## Public Contract
Raw/ODS 在 `raw/`，DWD 在 `dwd/`，DWS/ADS 在 `ads/`，模型在 `ml/`；Dashboard UI Contract 见
[24-DASHBOARD-API.md](../../docs/bigdata/24-DASHBOARD-API.md)。
## Future Implementation
为每个 Contract 增加版本、样例和自动校验。
## Acceptance
下游可只依据公开文档实现；破坏性变更有 CCR、迁移和回滚方案。
