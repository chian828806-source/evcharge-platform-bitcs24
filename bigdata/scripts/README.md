# Orchestration scripts

## Module Responsibility
提供 Pipeline、健康检查和演示编排入口。
## Owner
A（Integration）。
## Input
公开配置、批次号和各模块 CLI Contract。
## Output
可审计运行日志、dry-run 与非零失败码。
## Allowed Dependencies
各模块公开启动/健康检查接口。
## Forbidden Dependencies
写死凭据、删除一期数据库或实现业务/视觉功能。
## Public Contract
批次、输入路径、退出码和日志格式。
## Future Implementation
增加 `run_pipeline` 与 health-check。
## Acceptance
能显示批次和路径，任一步失败不伪造成功。
