# database/simulation — 公开数据模拟工作流

本目录把 Town of Cary CC0 充电会话保存为可追溯的历史事实，并生成服务端可导出的站点小时指标。它不会为公开数据伪造用户、钱包或支付记录。

## 完整构建

在仓库根目录执行：

```bash
.venv/bin/python -m database.simulation.build_cary_database \
  --input ml/data/raw/cary_ev_charging_sessions.csv \
  --database database/evcharge_cary_simulation.db \
  --from-date 2019-01-01

.venv/bin/python -m database.simulation.export_ml_history \
  --database database/evcharge_cary_simulation.db \
  --batch-no CARY-2019-V1 \
  --output ml/data/processed/station_hourly_load.csv
```

第一条命令以 `schema.sql` 和 `init_data.sql` 创建完整业务数据库，再向 `data_import_batch`、`charging_session_history` 和 `station_hourly_metric` 写入公开历史。第二条命令模拟未来 Qt/C++ 服务端的导出行为；生产环境应由服务端实现相同字段契约，Python ML 仍不得直接访问 SQLite。

## 表职责

| 表 | 内容 |
| --- | --- |
| `data_import_batch` | 来源、CC0 授权、SHA-256 和质量计数 |
| `charging_session_history` | 公开数据真实提供的开始时间、时长和电量 |
| `station_hourly_metric` | 连续小时负荷，直接导出给模型 |

生成数据库可以重复构建；脚本始终先写临时文件，通过完整性校验后再原子替换正式文件。
