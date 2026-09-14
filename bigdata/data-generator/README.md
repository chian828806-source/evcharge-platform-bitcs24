# Data generator

## Module Responsibility
按一期业务语义生成可复现 Raw CSV 并注入可审计质量问题。
## Owner
B（Data Pipeline）。
## Input
Raw Contract、固定 seed 和规模配置。
## Output
CSV、manifest、行数及 DQ 注入统计。
## Allowed Dependencies
一期 Schema 语义和 Raw Contract。
## Forbidden Dependencies
修改 `database/evcharge.db`、DWD/ADS/API/UI/ML。
## Public Contract
[22-DATA-CONTRACT.md](../../docs/bigdata/22-DATA-CONTRACT.md)。
## Future Implementation
实现固定 seed 生成与 1%–3% 问题注入。
## Acceptance
同一配置可重现，六类 DQ 问题和正常记录均可被质量模块检出。
