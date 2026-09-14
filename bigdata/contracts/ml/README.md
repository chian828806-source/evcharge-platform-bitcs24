# ML contract

Spark MLlib 只读取批准的 DWS 特征并把结果写入 `ads_prediction`。模型不得直接写 SQLite，
Vue 不得读取模型文件。完整输入、输出与评价规则见
[25-ML-CONTRACT.md](../../../docs/bigdata/25-ML-CONTRACT.md)。
