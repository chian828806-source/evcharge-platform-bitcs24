# Big-data tests

## Module Responsibility
保存 Contract、质量、API、Dashboard Core/UI 和 E2E 冒烟测试说明。
## Owner
A（Integration）；各模块 Owner 维护其测试。
## Input
小型、可提交、可重复的 Contract 样本。
## Output
测试结果和验收证据。
## Allowed Dependencies
公开 Contract、Mock 与测试 fixture。
## Forbidden Dependencies
大体量运行产物、真实凭据或跨层内部实现。
## Public Contract
22–27 号文档定义的可验证行为。
## Implemented Coverage
`test_warehouse_sql.py` 用最小 Spark DataFrame 验证正式营收、利用率和 DWS→ADS 口径；
`test_warehouse_api.py` 验证冻结端点、查询过滤、预测批次合并及 400/409 错误行为。其余测试覆盖
ML 输入、UrbanEV 适配和历史联调 API。
## Acceptance
不依赖生产数据即可验证数据层和 UI 层边界。
