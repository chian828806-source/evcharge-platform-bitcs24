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
## Run

在项目根目录执行：

```bash
python3 bigdata/data-generator/generate_raw.py \
  --output-dir output/raw/BD-20260915-001
```

生成器使用 `generator_config.json` 的固定随机种子生成六份 UTF-8 CSV 和 `manifest.json`，并按约
2% 的比例注入 DQ-001 至 DQ-006 异常。运行产物不得提交到 Git。
## Acceptance
同一配置可重现，六类 DQ 问题和正常记录均可被质量模块检出。
