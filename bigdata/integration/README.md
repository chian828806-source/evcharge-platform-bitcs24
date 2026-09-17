# Integration

## Module Responsibility
维护 E2E 编排、Contract 探针、演示清单与 Dashboard Core 集成。
## Owner
A（Architecture / Dashboard Core / Integration）。
## Input
B 的 DWD、C 的 API、E 的预测、Core 的公开 ViewModel。
## Output
可追溯 E2E 运行记录和验收证据。
## Allowed Dependencies
所有公开 Contract、Mock 数据和测试样本。
## Forbidden Dependencies
替代各 Owner 实现、手工 JSON 跳过失败步骤或视觉 UI 实现。
## Public Contract
27 号验收清单与 Dashboard Public UI Contract。
## 当前 Demo 实现

`run_full_demo.sh` 是合成数据 Contract 回归入口，顺序调用 B 已实现的生成、ODS、质量和 DWD
作业，再调用 C 的 `spark/warehouse/build_warehouse.py` 生成正式 DWS/ADS，最后运行 E 的 MLlib 作业。每一步使用
同一 `business_date` 和 `batch_id`，任一步失败都会停止，不能用手工 JSON 跳过。

```bash
bash bigdata/integration/run_full_demo.sh
```

真实数据演示使用 `bigdata/urbanev/run_pipeline.sh`。该入口从官方站点级 5 分钟 Raw 开始，
不调用随机生成器，并继续复用同一 ODS/DQ/DWD/DWS/ML/API 链路：

```bash
bash bigdata/urbanev/run_pipeline.sh /home/hadoop/datasets/UrbanEV 8
```

`build_demo_dws.py` 仅保留作历史联调参考，正式入口不再调用它。C 作业只读取公开的 DWD，
不修改 B 的任何输出。
`pipeline_demo_config.json` 将小时历史长度设为 35 天，满足 `lag_168`、24h 标签和
连续训练/验证/测试切分；它不覆盖 B 的默认生成器配置。

`../api/app.py` 按 [24-DASHBOARD-API.md](../../docs/bigdata/24-DASHBOARD-API.md)
提供九个冻结路径：`GET /api/v1/dashboard/{overview, energy-trend, revenue-trend,
station-ranking, pile-status, hourly-heatmap, station-utilization, prediction}`，以及
`GET /api/v1/data-quality/summary`。

服务每次请求都会读取 C 的 ADS 快照，并在批次一致时合并 E 的预测快照，因此作业发布新批次后
无需重启 Flask。快照不存在或损坏时返回 `409 BATCH_NOT_READY`，不会用联调样本冒充真实批次。

```bash
cd /home/hadoop/workspace/evcharge-platform-bitcs24-develop-ml
python3 bigdata/api/app.py
```

默认监听 `0.0.0.0:5000`，健康检查为 `GET /health`。
## Acceptance
`generator → ingestion → quality/DWD → warehouse → ML → API → Core → UI` 可按 batch 追溯。
