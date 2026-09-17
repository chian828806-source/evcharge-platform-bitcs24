# Flask API

## Module Responsibility
将 ADS 与 `ads_prediction` 以冻结的分析 REST 接口提供给 Dashboard Core。
## Owner
C（Warehouse / Analytics / Flask API）。
## Input
ADS、`ads_prediction` 与 24 号 HTTP Contract。
## Output
9 个只读 Flask 端点、统一 envelope 与错误行为。
## Allowed Dependencies
ADS Contract、Flask、API Contract。
## Forbidden Dependencies
取代 Qt/C++ 业务 Socket、写一期 SQLite、直接实现 Vue/UI。
## Public Contract
[24-DASHBOARD-API.md](../../docs/bigdata/24-DASHBOARD-API.md)；A 是直接 Consumer，D 不直连。
## Run

```bash
python3 bigdata/api/app.py
```

默认读取 C 发布的 `bigdata/runtime/warehouse/dashboard.json`，并仅在批次一致时合并 E 的
`bigdata/runtime/demo/dashboard.json` 预测结果。支持 `from`、`to`、`stationId`，非法查询返回
`400 INVALID_QUERY`，批次未发布返回 `409 BATCH_NOT_READY`。可通过 `EVCHARGE_ADS_SNAPSHOT`
和 `EVCHARGE_ML_SNAPSHOT` 覆盖路径。
## Acceptance
端点、空值、错误和 `meta.batchId` 通过契约测试；A 可不知 ADS 内部实现而消费。
