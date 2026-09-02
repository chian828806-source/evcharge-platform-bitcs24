# ml — 站点预测模块

本目录实现文档约定的三个预测结果：未来 `1h`、`6h`、`24h` 的站点负荷率、预计空闲桩数和高峰等级。Python 只交换 CSV/JSON 文件，不直接连接 SQLite；数据库读写与预测批次导入始终由 Qt/C++ 服务端负责。

## 数据流与边界

```text
Town of Cary 原始会话 CSV
        ↓ prepare_cary_data.py
连续 UTC 站点小时历史 CSV
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
| `data/raw/` | 原始数据，本地使用且不进入 Git |
| `data/processed/` | 连续 UTC 小时历史，本地使用且不进入 Git |
| `models/` | 训练模型，本地生成且不进入 Git |
| `output/` | 交给服务端导入的预测 JSON，本地生成且不进入 Git |
| `reports/` | 数据质量和模型评估报告；生成的 JSON 不进入 Git |
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

Windows PowerShell 激活命令为 `.venv\\Scripts\\Activate.ps1`。虚拟环境和所有大数据/模型产物都已忽略；本项目工作目录应继续放在 E 盘或虚拟机的 `/home/bit/workspace`。

## 1. 准备真实数据

```bash
python -m ml.prepare_cary_data \
  --input ml/data/raw/cary_ev_charging_sessions.csv \
  --output ml/data/processed/station_hourly_load.csv \
  --from-date 2019-01-01 \
  --report ml/reports/data_quality.json
```

输出使用连续 UTC 小时轴，避免夏令时产生重复或缺失小时。地址只通过 `config/station_mapping.json` 映射，不会根据全数据自动猜测站点容量。

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
