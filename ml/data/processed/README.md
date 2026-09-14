# processed — 小时级训练数据

本目录由模拟数据库的 `station_hourly_metric` 导出并随功能分支提交。每行代表一个项目映射站点的一个连续 UTC 小时。

核心字段：`timestamp`、`station_id`、`total_pile_count`、`session_starts`、`charging_minutes`、`energy_kwh`、`average_occupied_count`、`peak_occupied_count`、`average_available_count`、`station_load`。

`station_load` 是小时平均占用比例；模型通过本地站点真实桩数把预测负荷换算成预计空闲桩数。

生产环境由 Qt/C++ 服务端导出相同表头；本仓库中的 Python 导出器只用于模拟数据构建和契约测试。
