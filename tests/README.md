# tests — 自动化验证

本目录保存不依赖图形界面的自动化测试。测试构建产物应放在仓库外的临时构建目录，不能提交到Git。

## 子目录

| 目录 | 功能 |
| --- | --- |
| network | 公共协议、分帧、Session和Dispatcher测试 |
| `../ml/tests` | ML 特征、预测 JSON 契约和训练预测闭环测试 |
| database | Cary 历史入库、小时指标表和模型 CSV 导出测试 |

新增公共协议、拆分服务端/管理端通信目录或修改网络基础设施时，至少运行 network-protocol-tests。
修改 ML 数据字段、特征、模型选择或输出 JSON 时，至少运行 `python -m unittest discover -s ml/tests -v`。
修改数据库历史表或导出契约时，至少运行 `python -m unittest discover -s tests/database -v`。
