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
## Future Implementation
先实现 Contract Mock，再接入 ADS 查询。
## Acceptance
端点、空值、错误和 `meta.batchId` 通过契约测试；A 可不知 ADS 内部实现而消费。
