# 26. Ownership、Git 与契约变更

## 1. 工作分配

| 成员角色 | 责任 | 分支建议 |
| --- | --- | --- |
| A | 架构、Contract、Vue 大屏、集成与 Git Review | `feature/bigdata-foundation`、`feature/dashboard-v2` |
| B | Hadoop/HDFS、数据生成、ODS 摄取 | `feature/data-ingestion` |
| C | PySpark 质量检测与 DWD | `feature/data-quality` |
| D | SparkSQL DWS/ADS、Flask API | `feature/spark-warehouse` |
| E | Spark MLlib | `feature/spark-mllib` |

`feature/bigdata-foundation` 仅包含骨架、README、契约、Ownership、验收和接口占位，禁止混入
业务实现。它先合入 `develop`；其他分支从包含此基线的 `develop` 创建。

## 2. 规则

- A 不直接修改 B/C/D/E 内部实现；B 不修改 DWD；C 不修改 Raw；D 不修改 DWD 规则；E 不绕过
  数仓读取 Raw；任何跨 Owner 修复须先通知 Owner、PR 中说明原因并由其 Review。
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

建议合并序列：foundation → data-quality → spark-warehouse → spark-mllib → dashboard-v2 →
integration。真实 GitHub 用户名确定后，再将上述 Owner 映射写入 `.github/CODEOWNERS`；不能
用虚构账号制造看似生效的 CODEOWNERS 规则。
