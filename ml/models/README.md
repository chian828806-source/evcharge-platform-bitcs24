# models — 已训练模型

本目录保存通过时间切分测试的 `load_1h.joblib`、`load_6h.joblib` 和 `load_24h.joblib`。每个文件包含模型类型、horizon、特征顺序、训练时间、数据哈希和测试指标。

模型与 `ml/requirements.txt` 中的固定版本配套。修改训练数据、特征或依赖版本后必须重新训练三个文件，并同时更新 `ml/reports/training_report.json`；禁止只替换单个跨度的模型。
