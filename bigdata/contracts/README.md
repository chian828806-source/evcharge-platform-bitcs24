# Public contracts

这里的文档是模块之间唯一允许依赖的公开接口。下游可以消费版本化数据、表和 HTTP
响应，但不得导入上游 Python 模块、读取其临时路径或引用内部表。

- 原始 CSV / ODS：`raw/`，规范正文为 [22-DATA-CONTRACT.md](../../docs/bigdata/22-DATA-CONTRACT.md)。
- DWD：`dwd/`，清洗后的明细契约。
- DWS / ADS：`ads/`，指标口径及供 API、ML 的输出。
- 模型：`ml/`，特征与预测输出。

变更必须提交 CCR，写清当前定义、拟议定义、原因、上游/下游影响、兼容方案和回滚方案；
经 A Review 后同步修改本目录及 `docs/bigdata/`。破坏性变更必须增加版本，不得覆盖旧口径。
