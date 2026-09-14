# Flask API (D)

本模块未来只服务分析结果，不能替代 Qt/C++ 的业务 Socket 服务。端点、字段、单位、错误与空数据
行为已冻结在 [24-DASHBOARD-API.md](../../docs/bigdata/24-DASHBOARD-API.md)。实现先提供同契约
Mock，再接 ADS；不得让前端读取 SQLite 或 Spark 文件。
