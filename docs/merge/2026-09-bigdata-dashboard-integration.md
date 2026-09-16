# BigData & Dashboard V2 集成说明

## 合并基线

- 日期：2026-09-16（Asia/Shanghai）
- 合并前 `develop`：`16e47369e71a751eb4517bdd914edc6c190937ad`（`docs: propose DWD table schemas`）
- 功能合并后 `develop`：`feaaab28c8040e48e3d782b0490d50bf532ff6f6`（`merge: integrate BigData and Dashboard V2 baseline`）
- 本说明作为紧随功能合并的独立文档提交；最终 `develop` HEAD 以该文档提交为准。

## 纳入分支与 Git 关系

| 分支 | 实际 HEAD | 关系 |
| --- | --- | --- |
| `feature/dashboard-v2-core` | `5da3d58010935447104b3e96f3fdc9e09ec6d75f` | 被 E2E 分支包含 |
| `feature/bigdata-data-pipeline` | `dae20345b9712d1a50a02b5a2b2e57b777967e5f` | 被 E2E 分支包含 |
| `feature/bigdata-e2e-demo` | `7ffaf95097ba78ff3cc6bec202f8b14a0991ca44` | 包含两条前置分支的最终演进 |

验证得到：`merge-base(develop, dashboard)=0a948fe`，`merge-base(develop, pipeline)=16e4736`，`merge-base(develop, e2e)=16e4736`；`dashboard` 与 `e2e` 的 merge-base 为 `5da3d58`，`pipeline` 与 `e2e` 的 merge-base 为 `dae2034`。因此 `bigdata-e2e-demo` 已完整包含另外两个目标分支，采用一次 `--no-ff` merge，而没有重复 cherry-pick 或重复合并。

## 新增与修改内容

- **Dashboard V2 Core**：API client、DTO、adapter、DataSource 工厂、Mock/Real 选择、Pinia Store、Realtime WebSocket、模型和对应单测；`App.vue` 仅使用 `useDashboardStore()`，没有页面直接请求 Flask。
- **Dashboard 运行基础设施**：`main.ts` 注册 Pinia 与 DataV，`vite.config.ts` 保留 `/api -> http://127.0.0.1:5000` 开发代理；保留修正后的 `core/api/client.ts` fetch 绑定、超时与错误处理。
- **视觉 Demo**：保留可替换的 `App.vue`、`dashboard.css` 和通用 ECharts 生命周期组件。
- **BigData 链路**：数据生成、HDFS ODS 摄取、DQ、DWD、DWS bridge、Spark MLlib 负荷预测、dashboard snapshot、Flask REST API 和一键 demo 脚本。
- **可复现性**：新增 `bigdata/requirements.txt`，声明 Flask、Flask-Cors 与 PySpark 的运行/测试依赖。

重要已有文件的修改均为集成内容而非冲突解决：各 `bigdata/**/README.md` 补充运行契约/说明，`.gitignore` 增加运行产物忽略规则，`web-dashboard/README.md` 指向 V2。没有对 Qt Server、User、Admin、Device Simulator、一期 Dashboard 或协议做功能性改动。

## 文件级增删改统计

功能合并范围 `16e4736..feaaab2`：56 个文件，新增 5,235 行、删除 22 行。以下清单来自 `git diff --name-status`；本说明文件在随后的文档提交中新增。

### A — Added

```text
bigdata/data-generator/generate_raw.py
bigdata/data-generator/generator_config.json
bigdata/environment/B-PIPELINE-RUNBOOK.md
bigdata/environment/check_environment.sh
bigdata/ingestion/ingest_ods.py
bigdata/integration/build_demo_dws.py
bigdata/integration/demo_api.py
bigdata/integration/pipeline_demo_config.json
bigdata/integration/run_full_demo.sh
bigdata/ml/jobs/README.md
bigdata/ml/jobs/station_load_demo.py
bigdata/requirements.txt
bigdata/spark/dwd/build_dwd.py
bigdata/spark/quality/quality_check.py
bigdata/tests/test_demo_api.py
bigdata/tests/test_ml_pipeline_contract.py
web-dashboard/v2/.env.example
web-dashboard/v2/README.md
web-dashboard/v2/index.html
web-dashboard/v2/package-lock.json
web-dashboard/v2/package.json
web-dashboard/v2/src/App.vue
web-dashboard/v2/src/components/EChartView.vue
web-dashboard/v2/src/core/adapters/dashboardAdapter.test.ts
web-dashboard/v2/src/core/adapters/dashboardAdapter.ts
web-dashboard/v2/src/core/api/client.test.ts
web-dashboard/v2/src/core/api/client.ts
web-dashboard/v2/src/core/api/dto.ts
web-dashboard/v2/src/core/api/endpoints.ts
web-dashboard/v2/src/core/config/runtime.ts
web-dashboard/v2/src/core/datasource/DashboardDataSource.ts
web-dashboard/v2/src/core/datasource/createDataSource.test.ts
web-dashboard/v2/src/core/datasource/createDataSource.ts
web-dashboard/v2/src/core/datasource/flaskDataSource.ts
web-dashboard/v2/src/core/mocks/fixtures/dashboard.ts
web-dashboard/v2/src/core/mocks/mockDataSource.test.ts
web-dashboard/v2/src/core/mocks/mockDataSource.ts
web-dashboard/v2/src/core/models/dashboard.ts
web-dashboard/v2/src/core/realtime/qtDashboardWebSocket.test.ts
web-dashboard/v2/src/core/realtime/qtDashboardWebSocket.ts
web-dashboard/v2/src/core/stores/dashboard.test.ts
web-dashboard/v2/src/core/stores/dashboard.ts
web-dashboard/v2/src/main.ts
web-dashboard/v2/src/styles/dashboard.css
web-dashboard/v2/src/vite-env.d.ts
web-dashboard/v2/tsconfig.json
web-dashboard/v2/vite.config.ts
```

### M — Modified

```text
.gitignore
bigdata/data-generator/README.md
bigdata/environment/README.md
bigdata/ingestion/README.md
bigdata/integration/README.md
bigdata/ml/README.md
bigdata/spark/dwd/README.md
bigdata/spark/quality/README.md
web-dashboard/README.md
```

### D / R — Deleted or renamed

无。此次集成没有主动功能性文件删除或重命名。

## 冲突解决记录

无冲突。合并使用 Git `ort` 策略，目标 E2E 分支已以最新 `develop` 为祖先，且已包含 Dashboard V2 与数据管道提交；因此不需要以 `--ours` 或 `--theirs` 覆盖任何文件。

## 最终架构

```text
Raw Generator
      ↓
HDFS ODS
      ↓
DQ
      ↓
DWD
      ↓
DWS / Demo DWS Bridge
      ↓
Spark MLlib
      ↓
dashboard.json
      ↓
Flask /api/v1
      ↓
Dashboard Core (API Client → DataSource → Adapter)
      ↓
Pinia Store / ViewModel
      ↓
Vue / ECharts
```

## 验证结果

| 验证 | 结果 | 说明 |
| --- | --- | --- |
| `npm install` | 通过 | V2 锁定依赖成功安装；npm 报告 2 个 moderate audit 项，未执行破坏性 `audit fix --force`。 |
| `npm run test` | 通过 | 6 个 Vitest 文件、11 项测试均通过；覆盖 Mock/Real DataSource、Store、Adapter、API client、Realtime。 |
| `npm run build` | 通过 | `vue-tsc --noEmit && vite build` 成功；仅有 ECharts bundle 大小警告。 |
| Python 单测 | 环境受限 | `python -m unittest discover -s bigdata/tests -v` 未能导入：本机缺 `flask_cors` 与 `pyspark`；两次临时 pip 安装均因 PyPI TLS EOF 失败。依赖已写入 `bigdata/requirements.txt`。 |
| Python 语法 | 通过 | `python -m compileall -q bigdata` 成功。 |
| Flask API | 部分通过 | 在 Flask-Cors 缺失时，以仅替代 CORS 装饰的内存 shim 实例化应用；`/health` 与 9 个 Dashboard 接口共 10/10 返回 HTTP 200 JSON。真实 CORS 包与完整 Python 单测需在可安装依赖的环境复跑。 |
| Hadoop/Spark E2E | 环境受限 | 未找到 `hdfs`、`spark-submit`，未运行 `run_full_demo.sh`；未宣称 E2E 成功。 |

`git diff --check` 与全局冲突标记搜索均通过。

## 后续大屏开发边界

后续负责人可以重新设计 `web-dashboard/v2/src/App.vue` 与 `web-dashboard/v2/src/styles/dashboard.css`；如有必要，可改造 `src/components/EChartView.vue` 的视觉使用方式。必须保留并复用 `src/core/**`、`main.ts` 中 Pinia/DataV 注册、`vite.config.ts` 的 `/api` 代理、`core/api/client.ts`、`bigdata/integration/demo_api.py` 及全部 Hadoop/DWS/ML 代码。

页面和图表**不得直接请求 Flask**；所有 Dashboard 数据必须经 Dashboard Store / ViewModel 获取，保持 BigData → Flask → Dashboard Core → Pinia → View 的职责边界。
