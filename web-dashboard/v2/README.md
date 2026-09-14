# EVCharge Dashboard V2 Core

## 目的

本工程是 Phase 2 的 Dashboard 数据层，不是大屏 UI。它将 Flask 分析 API、Mock 和一期 Qt
WebSocket 分别接入，再发布稳定的 Pinia Store / `DashboardViewModel` 给 UI 使用。

```text
ADS → Flask → Core API/Adapter/Store → Public ViewModel → D: Vue + ECharts UI
Qt Dashboard WebSocket → Core realtime adapter ────────┘
```

## 技术栈与 V1 关系

- Vue 3、Vite、TypeScript、Pinia、Vitest、浏览器 `fetch`；不安装 ECharts。
- `../` 是一期稳定的原生 JavaScript Dashboard V1，保持原样、可独立运行。
- `v2/` 是新的 Core 工程；`App.vue` 仅为开发验证 Shell，不是正式页面。

## 目录

```text
src/core/
├── api/          # Flask endpoints、DTO、错误归一化
├── adapters/     # DTO → ViewModel，不重新计算业务 KPI
├── config/       # VITE 运行配置
├── datasource/   # Flask / Mock 的统一 DashboardDataSource
├── mocks/        # 严格符合 24 号 API Contract 的 fixture
├── models/       # Public UI Contract
├── realtime/     # 复用 V1 Qt WebSocket envelope/topic/reconnect
└── stores/       # Pinia 状态与加载 action
```

## A / D Ownership

A 负责本目录的 `core/`：API Client、DataSource、Adapter、Store、Mock/Real、Realtime、ViewModel、
Integration。A 不负责 CSS、页面、布局、组件、ECharts 或视觉设计。

D 后续只在 `src/views/`、`src/components/`、`src/charts/`、`src/styles/` 实现 UI；这些目录不由本
分支创建。D 只能读取 Store / `DashboardViewModel`，不得直接调用 Flask 或 Qt WebSocket，不得读取
SQLite/HDFS/Spark，也不得重新计算 KPI。

## 配置与运行

```bash
cd web-dashboard/v2
npm install
npm run dev
npm run typecheck
npm run test
npm run build
```

| 变量 | 默认 | 作用 |
| --- | --- | --- |
| `VITE_DATA_MODE` | `mock` | `mock` 使用本地 Contract fixture；`real` 使用 Flask |
| `VITE_API_BASE_URL` | 空 | real mode 必填，例如 Flask 的 `/api/v1` 根地址 |
| `VITE_API_TIMEOUT_MS` | `10000` | fetch timeout |
| `VITE_REALTIME_ENABLED` | `false` | 是否启用一期 Qt WebSocket |
| `VITE_REALTIME_WS_URL` | 空 | real-time 地址，例如 `ws://host:18081/dashboard` |

Mock Mode 可完整运行。Real Mode 只调用 C 将来实现的 API；服务不存在时 Store 以 `error` 状态表达
连接失败，不会伪造 Flask/Spark/ADS 结果。Realtime 是独立资源，不会静默覆盖历史分析 KPI。

## DataSource、Store 与 ViewModel

`DashboardDataSource` 覆盖 24 号文档的 overview、energy/revenue trend、station ranking、pile
status、hour heatmap、station utilization、prediction、data-quality 九项读取。Store 为每项暴露：
`data`、`status`（idle/loading/success/empty/error）、`error`、`lastUpdated`，以及
`loadXxx()`、`refreshAll()`、`setDataMode()`。

`DashboardViewModel` 同时包含九项资源、`source` 和独立 `realtime` 状态。API DTO 只在
`api/dto.ts` 使用；`dashboardAdapter.ts` 负责字段的 null-safe 整理，不计算营收、订单、电量或
利用率。

## 测试

Vitest 覆盖 Mock Contract、DataSource 模式选择、DTO Adapter、HTTP 错误归一化、Store success /
empty / error 以及 V1 Qt WebSocket 消息解析。

正式契约以 `docs/bigdata/24-DASHBOARD-API.md` 为准；如 C 的 Flask 实现与该文档冲突，应发起 CCR，
不得在 Core 中自行改变字段或单位。
