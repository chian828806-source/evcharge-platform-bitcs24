# Integration (A)

维护端到端运行清单、契约探针和演示样本。集成顺序为 `generator → ingestion → quality/DWD →
warehouse → ML → API → dashboard`；任一步无输出或契约不符即失败，不能用手工 JSON 跳过。
