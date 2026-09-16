# ML jobs

本目录保存 E 负责的 Spark MLlib 作业。

`station_load_demo.py` 只有一个数据入口：标准 `dws_station_hour`。它负责契约校验、
时间特征与滞后特征、连续时间切分、GBT 与 `lag_168` baseline 对比、1h/6h/24h
预测和 `ads_prediction` 发布。

Raw、ODS、质量治理、DWD 和 DWS 的生成均在作业外完成，因此训练代码不会绕过队友管道。
