# UrbanEV 真实数据接入

本目录把论文公开的 **UrbanEV 站点级 5 分钟原始数据** 接入 EVCharge 第二阶段流水线。
它不会调用 `data-generator/generate_raw.py`，也不会让 ML 绕过数仓直接读取论文 CSV。

```text
UrbanEV station Raw (5 min)
  → prepare_raw.py（外部格式映射、小时聚合、可追溯模拟维度补全）
  → 项目 Raw CSV + manifest
  → HDFS ODS
  → PySpark DQ accepted/rejected
  → DWD
  → DWS station-hour
  → Spark MLlib 1h/6h/24h
  → ads_prediction + dashboard.json
  → Flask REST
  → Dashboard Core
  → Vue/ECharts UI
```

## 1. 下载数据集

数据源：Li 等人的 UrbanEV，DOI `10.5061/dryad.np5hqc04z`。

- 官方数据页：<https://datadryad.org/dataset/doi:10.5061/dryad.np5hqc04z>
- 论文代码：<https://github.com/IntelligentSystemsLab/UrbanEV>
- 论文：<https://doi.org/10.1038/s41597-025-04874-4>

论文 GitHub README 同时给出了 Google Drive 与百度网盘官方镜像。当 Dryad 下载需要授权或
无法稳定续传时，可以使用论文仓库列出的镜像；不要使用来源不明的二次打包文件。不同发布
时间的官方压缩包大小可能不同，本次在 2026-09-16 从论文仓库所列 Google Drive 镜像取得的
`UrbanEVDataset.zip` 为 `647305188` 字节，SHA-256 为
`9d3f0aec34434546d082509efcdeeaea116eaa841701cc5854a9b62a27881a79`，并已通过 `unzip -t`。
此哈希只对应该镜像当时版本，不能用于否定 Dryad 后续版本。

在 Dryad 页面选择最新版本的 `UrbanEVDataset.zip`。当前官方页面标注的 2026-02-04
版本约 320 MB。Dryad 可能要求在浏览器中完成下载；不要把网页登录信息或临时下载地址写入脚本。

数据集不进入 Git。建议放到虚拟机独立数据目录：

```bash
mkdir -p /home/hadoop/datasets/UrbanEV
unzip ~/Downloads/UrbanEVDataset.zip -d /home/hadoop/datasets/UrbanEV
```

完整压缩包展开约 15.6 GB；还要为 HDFS、Spark 临时目录和模型预留空间，完整导入前建议至少
准备 25 GB 可用空间。教学虚拟机空间不足时，先只提取 `station_information.csv`、
`pile_rated_power.csv` 和少量 `station-raw/charge_5min/<station_id>.csv` 做 8 站点验收，
不能用已经清洗的 zone 聚合文件代替 Raw。

解压后必须能找到下面三个来源：

```text
20220901-20230228_station-raw/
├── charge_5min/
│   ├── 1001.csv
│   ├── 1002.csv
│   └── ...
├── station_information.csv
└── pile_rated_power.csv
```

检查命令：

```bash
find /home/hadoop/datasets/UrbanEV -type d -path '*station-raw/charge_5min'
find /home/hadoop/datasets/UrbanEV -type f \
  \( -name station_information.csv -o -name pile_rated_power.csv \)
```

`zone-cleaned-aggregated/charge_1hour` 是论文已经清洗聚合的成品，不能替代本流程的 Raw 输入。
论文 GitHub 仓库可用于阅读基准模型，但运行本项目不要求复制论文模型代码。

## 2. 首次导入与联调

先启动 HDFS。进入项目根目录后，建议先选容量最大的 8 个站点跑通：

```bash
bash bigdata/urbanev/run_pipeline.sh /home/hadoop/datasets/UrbanEV 8
```

参数依次为：解压目录、导入站点数、可选批次号。站点数为 `0` 时导入全部官方站点：

```bash
bash bigdata/urbanev/run_pipeline.sh \
  /home/hadoop/datasets/UrbanEV \
  0 \
  URBANEV-20230228-FULL-V1
```

完整导入会处理约 1,682 个站点和六个月的 5 分钟记录，第一次运行明显慢于 8 站点演示。
生成的本地 Raw、模型和快照位于 `bigdata/runtime/`，该目录已被 `.gitignore` 排除。

## 3. 外部字段如何进入项目契约

| UrbanEV Raw | EVCharge Raw / DWS | 处理口径 |
| --- | --- | --- |
| `time` | `hour_start` | Asia/Shanghai，向下取整到小时 |
| `busy` | `average_occupied_count` | 同一小时有效 5 分钟样本均值 |
| `idle` | `average_available_count` | 同一小时有效样本均值 |
| `busy` 的正向变化 | `session_starts` | 会话开始次数的可解释代理量，不宣称为真实订单 |
| `duration` | `charging_pile_minutes` | 小时内求和后由小时乘 60 |
| `volume` | `energy_kwh` | 小时内求和，使用官方实际估计电量 |
| `busy / charge_count` | `station_load` | 0–1 负荷口径 |
| `e_price / s_price` | 站点电价/服务费 | 站点全期非负值中位数，元转分 |
| `station_information` | `stations.csv` | 坐标、容量和 TAZ 直接映射 |
| `pile_rated_power` | `piles.csv` | DC→FAST、AC→SLOW，保留额定功率 |

UrbanEV 没有本项目的用户、订单和逐会话记录。为了让数据库、DWD 和联调接口具备完整主外键，
适配器默认用固定 seed 生成 500 个匿名用户，再把每个有效站点小时的官方电量和充电时长拆分到
模拟订单与会话。用户分配、桩分配和时间细节是模拟的；站点小时 `energy_kwh`、
`charging_pile_minutes` 和 `station_load` 原值不变。订单电量以 `0.000001 kWh` 为单位守恒拆分，
会话时长以秒为单位守恒拆分；订单表受冻结契约限制，`charge_minutes` 取整为正整数。若极少数小时
有电量/占用却没有时长，适配器只为关系完整性补 60 秒，并在 manifest 的
`synthetic_duration_imputed_seconds` 单独计数，官方小时指标仍不改写。所有模拟订单都带
`URBANEV-SYN-` 前缀，manifest 的 `synthetic_dimensions` 会记录 seed、模拟字段和不变量。

因此 `session_starts` 与 `orderCount` 是可复现的模拟会话量，不能称为真实已支付订单；
`URBANEV-SYN-` 订单也必须从正式营收口径排除。ML 特征只使用官方负荷的历史窗口、时间特征和
站点容量，不使用随机 user ID、order ID 或桩分配。UrbanEV 没有历史逐桩在线状态，`piles.csv`
中的状态仅用于维度契约；大屏当前桩位结构来自站点小时 `busy/idle`，不是设备心跳。

需要改变模拟规模或复现种子时，可先单独执行适配器：

```bash
python3 bigdata/urbanev/prepare_raw.py \
  --dataset-root /home/hadoop/datasets/UrbanEV \
  --output-dir bigdata/runtime/urbanev/raw/URBANEV-20230228-RAW-V1 \
  --batch-id URBANEV-20230228-RAW-V1 \
  --max-stations 8 \
  --synthetic-users 500 \
  --synthetic-seed 20260916
```

原始小时缺少足够样本、字段为空、数值为负或 `busy + idle` 超过容量时，适配器会输出一条
可追溯但字段不完整的项目 Raw 行。它随后由现有 DQ 作业拒绝并进入 rejected，不在适配器里
静默填补，也不会泄漏到 ML。

站点选择只接受同时存在于 `station_information.csv`、`pile_rated_power.csv` 和
`charge_5min/<station_id>.csv` 的完整站点，并在应用 `--max-stations` 限制前完成交集过滤。
本次实测发现站点 `1077` 有站点信息和时序文件、但缺少桩功率记录，因此被自动跳过；这属于
源数据完整性处理，不应伪造额定功率补齐。

## 4. 启动 Flask 和真实大屏

流水线完成后开两个终端：

```bash
python3 bigdata/api/app.py
```

```bash
cd web-dashboard/v2
cp .env.urbanev.example .env.local
npm install
npm run dev -- --host 0.0.0.0
```

浏览器打开 `http://<虚拟机IP>:5173/`。`.env.urbanev.example` 会让 UI 通过 Vite 的 `/api`
代理读取 Flask；页面上的数据源状态应显示“数据正常”，而不是“Mock 数据”。

必须在启动 Vite **之前** 创建 `.env.local`，否则前端默认使用 Mock 夹具，只会显示
`2026-09-14` 的单点样例。修改环境变量后需要重启 Vite，单纯刷新浏览器不会重新读取启动环境。
Flask 每次请求都会重新读取 C 的 ADS 快照，并在批次一致时合并 E 的预测快照，所以重新计算或
训练后不必重启 Flask。当前演示关闭 Qt WebSocket 实时推送，展示的是最近一次成功批处理快照。

## 5. 验收

```bash
python3 -m unittest bigdata.tests.test_urbanev_adapter
python3 -m unittest discover -s bigdata/tests -p 'test_*.py'
npm --prefix web-dashboard/v2 test
npm --prefix web-dashboard/v2 run build
```

还应检查以下批次一致：

```bash
hdfs dfs -ls /evcharge/ods/_manifests/dt=2023-02-28/
hdfs dfs -ls /evcharge/quality/reports/dt=2023-02-28/
hdfs dfs -ls /evcharge/dwd/_manifests/dt=2023-02-28/
grep -n 'URBANEV-' bigdata/runtime/demo/dashboard.json | head
```

## 6. 已验证批次与结果

2026-09-16 已在 `node100` 大数据虚拟机使用 Hadoop 3.3.0、Spark 3.4.1、Python 3.11.11
执行批次 `URBANEV-20230228-OFFICIAL-SMOKE-V1`：

| 项目 | 结果 |
| --- | ---: |
| 真实站点 | 8 |
| 桩维度 | 872 |
| 官方 5 分钟观测 | 417,024 |
| 生成的匿名用户 | 500 |
| 可关联模拟订单/会话 | 124,219 / 124,219 |
| 站点小时 Raw | 34,752 |
| DQ 接受 / 拒绝小时 | 22,943 / 11,809 |
| DWD 订单 / 会话 / 桩 / 站点小时 | 124,219 / 124,219 / 872 / 22,943 |

ML 使用严格时间切分，并与周期基线比较：

| 预测跨度 | GBT MAE | GBT RMSE | GBT R² | 周期基线 MAE | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| 1h | 0.0398 | 0.0574 | 0.8980 | 0.0526 | GBT 优于基线 |
| 6h | 0.0581 | 0.0776 | 0.8125 | 0.0514 | 测试集上基线更好 |
| 24h | 0.0549 | 0.0745 | 0.8285 | 0.0481 | 测试集上基线更好 |

因此只能宣称“完整流水线与 1h 预测有效”，不能宣称三个跨度都超过基线。6h/24h 结果提示
仍需调参、加入天气/价格/空间邻接特征，或采用更合适的时空模型。前端 12 项 Vitest、Python
适配/API 测试及 Vue 生产构建均已通过；Vite 构建的大包体积警告不影响本次演示，但后续可用
动态导入拆分 ECharts。
