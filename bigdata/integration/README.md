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
## Future Implementation
增加 contract/E2E 检查器。
## Acceptance
`generator → ingestion → quality/DWD → warehouse → ML → API → Core → UI` 可按 batch 追溯。
