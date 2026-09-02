# raw — 原始公开数据

当前冻结输入为 Town of Cary 的 Electric Vehicle Charging Stations CSV，并随功能分支提交，保证数据库和模型可以离线复现。

- 官方目录：https://catalog.data.gov/dataset/electric-vehicle-charging-stations
- CSV：https://data.townofcary.org/api/v2/catalog/datasets/electric-vehicle-charging-stations/exports/csv
- 授权：CC0 1.0 Universal
- 本地文件：`cary_ev_charging_sessions.csv`
- SHA-256：`d2de65828a5a11600ed89813f929c096a56ca069be118a94a50d688c79e42de9`
- 原始记录：20,142 条；2019 年后映射并接受 14,607 条

上游数据更新时不得直接覆盖冻结文件；应更新 SHA-256、质量报告、模拟数据库、处理结果和三个模型，并作为同一变更提交。
