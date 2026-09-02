# ml — 站点预测模块

本目录实现文档约定的三个预测结果：未来 `1h`、`6h`、`24h` 的站点负荷率、预计空闲桩数和高峰等级。Python 只交换 CSV/JSON 文件，不直接连接 SQLite；数据库读写与预测批次导入始终由 Qt/C++ 服务端负责。

## 数据流与边界

```text
Town of Cary 原始会话 CSV
        ↓ database/simulation/build_cary_database.py
SQLite 会话历史 + 站点小时指标
        ↓ database/simulation/export_ml_history.py
标准连续 UTC 站点小时历史 CSV
        ↓ train.py
三个模型文件 + 独立测试集评估报告
        ↓ predict.py
predictions.json
        ↓ 服务端校验并用一个事务导入
SQLite prediction 表 → Qt 客户端 / Web 大屏
```

- `predictedLoad` 是未来窗口平均负荷率，范围为 `0..1`。
- `predictedAvailableCount = round(站点桩数 × (1 - predictedLoad))`，并限制在合法范围内。
- `peakLevel` 由负荷率统一派生：`LOW < 0.4`、`0.4 ≤ MEDIUM < 0.7`、`HIGH ≥ 0.7`。
- 当前真实数据没有逐桩故障/离线标签，因此“空闲桩”是基于负荷的容量估计，不等同于服务端实时状态；界面展示时应标注为预测值。

## 目录

| 路径 | 作用 |
| --- | --- |
| `config/` | 外部数据站点到项目 `stationId` 的显式映射 |
| `data/raw/` | CC0 原始数据；当前冻结版本随仓库发布 |
| `data/processed/` | 从模拟数据库导出的标准连续 UTC 小时历史 |
| `models/` | 与固定 Python 依赖配套的 1h/6h/24h 已训练模型 |
| `output/` | 交给服务端导入的预测 JSON，本地生成且不进入 Git |
| `reports/` | 随模型提交的数据质量和独立测试集评估报告 |
| `tests/` | 特征、契约和端到端自动化测试 |
| `prepare_cary_data.py` | 清洗并聚合 Town of Cary 会话 |
| `features.py` | 训练和预测共用的无泄漏特征 |
| `train.py` | 时间切分、模型选择、测试集评估 |
| `predict.py` | 加载模型并生成预测批次 |
| `contracts.py` | 在写出前校验服务端导入契约 |

## 环境

在仓库根目录执行：

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r ml/requirements.txt
```

Windows PowerShell 激活命令为 `.venv\\Scripts\\Activate.ps1`。虚拟环境和运行时预测批次已忽略；冻结的数据、模型和报告随功能分支提交，便于队友直接复现。

## 1. 从公开数据构建模拟数据库并导出模型输入

```bash
.venv/bin/python -m database.simulation.build_cary_database \
  --input ml/data/raw/cary_ev_charging_sessions.csv \
  --database database/evcharge_cary_simulation.db \
  --from-date 2019-01-01

.venv/bin/python -m database.simulation.export_ml_history \
  --database database/evcharge_cary_simulation.db \
  --batch-no CARY-2019-V1 \
  --output ml/data/processed/station_hourly_load.csv
```

模拟数据库保存来源会话和连续小时指标。导出使用 UTC 小时轴，避免夏令时产生重复或缺失小时。地址只通过 `config/station_mapping.json` 映射，不会根据全数据自动猜测站点容量。

## 2. 训练与评估

```bash
python -m ml.train \
  --input ml/data/processed/station_hourly_load.csv \
  --model-dir ml/models \
  --report ml/reports/training_report.json
```

每个站点分别按时间顺序取前 70% 训练、随后 15% 验证、最后 15% 测试，不能随机打乱。验证集用于在 `HistGradientBoostingRegressor` 和“七天前同时间”基线之间选择；测试集只用于最终报告。模型若不能在验证集上至少改善 1%，自动退回稳定的季节基线。

## 3. 生成服务端导入文件

```bash
python -m ml.predict \
  --history ml/data/processed/station_hourly_load.csv \
  --model-dir ml/models \
  --batch-id 20260902T120000Z-cary-v1 \
  --output ml/output/20260902T120000Z-cary-v1/predictions.json
```

输出严格匹配 `docs/03-API.md` 第 14 节。服务端还必须验证站点存在性，并保证整批成功或整批回滚；Python 端无法代替这两项数据库责任。

## 4. 自动化测试

```bash
python -m unittest discover -s ml/tests -v
```

测试使用合成小时序列，不依赖数 MB 的真实数据，所以队友克隆仓库后可以直接执行。正式联调前还应使用服务端导出的真实历史再跑训练与导入测试。

## 当前数据能力边界

现有 Cary 数据可用于学习站点负荷的小时、星期与历史滞后规律，也能据站点容量推导预计空闲数和高峰等级。它不能可靠预测单个项目充电桩的故障、离线、预约占用或用户行为，因为原始数据没有这些标签；这些实时状态应由 Qt/C++ 业务后端提供，不能由 ML 编造。
