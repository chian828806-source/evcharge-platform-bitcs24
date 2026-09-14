# 26. Ownership、Git 与契约变更

## 1. 工作分配

| 成员角色 | 责任 | 分支建议 |
| --- | --- | --- |
| A | 架构、Contract、Dashboard Core、集成与 Git Review；不负责页面设计 | `feature/dashboard-v2-core` |
| B | Hadoop/HDFS、数据生成、ODS、PySpark 质量检测、清洗与 DWD | `feature/bigdata-data-pipeline` |
| C | SparkSQL、DWS/ADS、运营分析与 Flask API | `feature/spark-warehouse-api` |
| D | Vue 页面、组件、ECharts、CSS、布局与交互；不直连 Flask | `feature/dashboard-v2-ui` |
| E | Spark MLlib | `feature/spark-mllib` |

`feature/bigdata-foundation` 仅包含骨架、README、契约、Ownership、验收和接口占位，禁止混入
业务实现。它先合入 `develop`；其他分支从包含此基线的 `develop` 创建。

## 2. 规则

- A 不直接修改 B/C/E 内部实现或 D 的视觉层；B 不修改 DWS/ADS/API/UI/ML；C 不修改 Raw/DWD
  清洗或 UI；D 不修改 Dashboard Core 或后端契约；E 不绕过数仓读取 Raw。任何跨 Owner 修复须先
  通知 Owner、PR 中说明原因并由其 Review。
- A 与 D 可同时涉及 `web-dashboard`，但不能无边界共改：A 负责 `src/core/{api,store,adapters,
  realtime,mocks,config}`，D 负责 `src/{views,components,charts,styles}`。公共 UI Contract 修改
  须双方确认，由 A 最终 Review；A 不在 Core 分支实现 CSS、页面布局或 ECharts 视觉 option。
- 不修改一期冻结目录 `qt-user/`、`qt-admin/`、`qt-server/`、`qt-device-simulator/`、
  `database/schema.sql` 和一期稳定协议，除非 CCR 获批。
- PR 必须列出 SRS 编号、影响的 Contract、测试、数据样本/批次、兼容性与未完成项；只暂存本任务
  文件，禁止用 `git add .`。

## 3. CCR 模板

```text
CCR-ID:
申请人 / 日期:
涉及 Contract 与当前版本:
当前定义 → 拟议定义:
原因与收益:
上游 / 下游影响:
兼容与迁移方案:
回滚方案:
Review 结论:
```

Foundation 合入最新 `develop` 后，B/C/A/D 可按契约并行：B 交付 DWD 后 C 接入；C 的 API Mock
或 A 的 Mock ViewModel 到位后 D 可并行；E 在 DWS Contract 冻结后并行，最终路径为
`E → ads_prediction → C Flask → A Core → D UI`。这表示数据依赖，不表示所有成员串行等待。
真实 GitHub 用户名确定后，再将上述 Owner 映射写入 `.github/CODEOWNERS`；不能用虚构账号制造
看似生效的 CODEOWNERS 规则。
