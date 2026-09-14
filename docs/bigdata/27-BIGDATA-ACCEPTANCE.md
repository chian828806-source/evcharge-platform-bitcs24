# 27. 第二阶段验收与演示

## 1. 验收矩阵

| 验收项 | 证据 |
| --- | --- |
| HDFS / ODS | `hdfs dfs -ls` 截图/日志、manifest 和批次路径 |
| 模拟与问题注入 | 固定 seed、输入行数、各问题类型计数 |
| PySpark 质量与 DWD | DQ 报告、rejected 样例、清洗前后行数 |
| 四层数仓 | 建表/ETL 日志、表行数、23 号文档血缘 |
| SparkSQL 分析 | 站点、区域、时段、营收/电量/利用率查询结果 |
| Dashboard Core（A） | API Client 契约测试、Mock/Real 切换、统一 ViewModel、Store 的 Loading/Empty/Error、Prediction 和 Qt WebSocket adapter |
| Dashboard UI（D） | 可运行 Vue3 页面、公开 ViewModel 消费、KPI/图表、Loading/Empty/Error 表现；无 Flask/SQLite/HDFS/Spark 直连或 KPI 重算 |
| Spark MLlib | 时间切分、模型指标、baseline 对比、预测样例 |
| 全链路 | 从一个 batch 到 API `meta.batchId` 的运行日志 |

## 2. 演示顺序

1. 显示一期业务系统和冻结边界；说明 Phase 2 只消费其数据语义。
2. 运行固定 seed 的生成/注入，展示 HDFS ODS manifest。
3. 展示质量报告、拒绝原因和 DWD 有效记录。
4. 执行/展示 SparkSQL 四层表及 ADS 指标。
5. 展示 MLlib 评价和 `ads_prediction`。
6. 调用 Flask 的 overview、trend、prediction、quality API，展示 Core 的统一 ViewModel，最后展示
   仅消费该 ViewModel 的 Vue 大屏。

## 3. 通过条件

同一批次在质量报告、ADS、API `meta.batchId` 和演示日志中可关联；所有数值遵循 22/23/24/25
号文档；异常输入有可预期拒绝而非静默成功；Dashboard UI 无需知道数据来自 Flask、Mock 还是
WebSocket；一期功能不被本分支修改或破坏。
