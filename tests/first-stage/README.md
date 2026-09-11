# first-stage — 第一阶段六模块验证

本目录把第一阶段测试用例表中的 48 个编号映射到仓库中的自动化测试或代码契约，并提供一个统一入口。范围仅包含用户账户、站点地图、订单充电、管理端、网络会话和设备通信，不包含 ML 与数据大屏。

## 文件

| 文件 | 用途 |
| --- | --- |
| `case-matrix.json` | 六张表、48 个用例编号与代码证据的追踪关系 |
| `test_first_stage_contracts.py` | 检查编号完整性、证据文件、关键符号以及数据库初始化 |
| `run-first-stage.sh` | 构建并运行服务端、Network、Admin、Integration 和契约测试 |

## 运行

在仓库根目录执行：

```bash
bash tests/first-stage/run-first-stage.sh
```

构建结果默认写入仓库同级的 `build-first-stage-tests`，不会污染 Git 工作区。`case-matrix.json` 中的 `automated` 表示行为由对应测试程序验证，`source-contract` 表示该用例依赖界面或外部环境，当前阶段验证其实现入口和协议接线保持存在。
