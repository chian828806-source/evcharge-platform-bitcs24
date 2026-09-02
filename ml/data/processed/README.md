# processed — 小时级训练数据

本目录由 `prepare_cary_data.py` 生成，不提交 Git。每行代表一个项目映射站点的一个连续 UTC 小时。

核心字段：`timestamp`、`station_id`、`source_port_count`、`session_starts`、`charging_minutes`、`energy_kwh`、`average_occupied_count`、`peak_occupied_count`、`average_available_count`、`station_load`。

`station_load` 是小时平均占用比例；模型通过本地站点真实桩数把预测负荷换算成预计空闲桩数。
