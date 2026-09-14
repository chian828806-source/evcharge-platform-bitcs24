# 25. Spark MLlib 契约

## 1. 输入与特征

正式模型只读取 `dws_station_hour`（或其版本化 `dws_ml_feature`），每行粒度为站点小时。
基础字段为 `station_id, timestamp, total_pile_count, session_starts, energy_kwh, station_load,
average_available_count`。可派生 `hour, day_of_week, is_weekend, lag_1, lag_24, lag_168,
rolling_mean_24`；所有 lag 和窗口只使用目标时间之前的数据，首段不足窗口的行不得泄漏未来值。

训练、验证、测试按时间连续切分，不能随机打散。模型、特征版本、训练范围、批次和随机种子必须
写入模型元数据。允许 `GBTRegressor` 或 `RandomForestRegressor`，但需与朴素 baseline 比较。

## 2. 输出

输出到 `ads_prediction`，由 C 的 Flask 查询并交给 A 的 Dashboard Core，不直接写一期 SQLite。
主键为
`station_id + prediction_time + horizon + model_version`，字段如下：

| 字段 | 规则 |
| --- | --- |
| `station_id`、`prediction_time` | 目标站点和目标小时 |
| `horizon` | 仅 `1h`、`6h`、`24h` |
| `predicted_load` | 0–1；沿用一期利用率口径 |
| `predicted_available_count` | 非负且不超过 `total_pile_count` |
| `peak_level` | `LOW/MEDIUM/HIGH`；阈值与版本写入元数据 |
| `model_name`、`model_version`、`generated_at` | 可追溯模型与运行 |
| `mae`、`rmse`、`r2` | 同一时间测试集上的评价值 |

一期 `ml/` 继续保留为业务 baseline；第二阶段的技术实现为 Spark MLlib，业务目标不重新定义。

## 3. 验证

每次训练至少输出样本数、时间范围、特征清单、缺失处理、MAE、RMSE、R²、baseline 对比和预测
样例。模型输出不合范围、特征版本不匹配或测试指标缺失时，批次不得发布到 API。
