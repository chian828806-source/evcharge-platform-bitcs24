# Ingestion (B)

负责将 Raw Contract 文件按 `dataset/dt=YYYY-MM-DD` 写入 HDFS ODS，并生成可审计的摄取清单：
源文件哈希、批次号、行数、路径和时间。摄取失败不得产生“已成功”标记。
