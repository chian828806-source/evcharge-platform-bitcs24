# database — 数据库脚本与说明

本目录存放 EVCharge 平台的 SQLite 数据库脚本。规范来源：[docs/04-DATABASE.md](../docs/04-DATABASE.md)、[docs/00-SRS-V1.0.md](../docs/00-SRS-V1.0.md) 第 6 节。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `schema.sql` | 正式建表脚本：8 张核心表 + 索引，可重复执行（先 DROP 后 CREATE） |
| `init_data.sql` | 演示/种子数据：默认管理员、示例用户、站点、电桩、历史订单、预测结果、日志 |
| `evcharge.db` | 运行时数据库文件（由下方命令生成，**不提交 git**） |

## 快速开始

```bash
# 1. 在仓库根目录执行：建库 + 导入种子数据
sqlite3 database/evcharge.db ".read database/schema.sql"
sqlite3 database/evcharge.db ".read database/init_data.sql"

# 2. 验证
sqlite3 database/evcharge.db ".tables"
sqlite3 database/evcharge.db "SELECT id, phone, nickname, balance_fen, status FROM user;"
```

生成 `evcharge.db` 后拷贝到 Qt PC 服务与管理端的工作目录即可使用（默认连接名建议 `main_connection`，多线程规则见 docs/04-DATABASE.md 第 11 节）。

## 默认账号

| 角色 | 账号 | 密码 | 说明 |
| --- | --- | --- | --- |
| 管理员 | `admin` | `123456` | 加盐 SHA-256 存储（盐 `evcharge`），不存明文 |
| 用户 | `13800000001` ~ `13800000004` | 无密码 | 手机号登录；新手机号自动注册 |

## 表清单（8 张）

| 表 | 用途 | 关键约束 |
| --- | --- | --- |
| `user` | 用户资料与钱包 | `phone` 唯一 |
| `admin` | 管理员账号 | `username` 唯一 |
| `charging_station` | 充电站 | — |
| `charging_pile` | 充电桩 | `pile_no` 唯一；6 态状态机 |
| `charging_order` | 充电订单 | 活动订单互斥：每用户 ≤1、每桩 ≤1（BR-003/BR-004） |
| `recharge_record` | 充值流水 | `balance_after_fen` 余额快照便于对账 |
| `prediction` | ML 预测结果 | `predicted_load` 0~1，统一负荷口径 |
| `operation_log` | 操作日志 | 远程重启等操作留痕（FR-IOT-001） |

## 关键业务口径（与 SRS 对齐）

- 金额一律以**分**存储：`balance_fen` / `amount_fen` / `price_fen_per_kwh`；展示时 ÷100 转元；
- 计费公式：`amount_fen = energy_kwh × price_fen_per_kwh`（FR-C-007）；
- 活动订单 = 状态 ∈ {`CREATED`, `CHARGING`, `PENDING_PAYMENT`}（FR-C-001）；
- 空闲数 = `AVAILABLE` 电桩数；站点负荷 = `chargingPileMinutes / (totalPileCount × windowMinutes)`（FR-ML-002）；
- 营收只统计 `COMPLETED` 订单（FR-A-002 / BR-006）。

## 变更流程

表结构变更时，必须按 docs/04-DATABASE.md 第 14 节同步修改：

1. `database/schema.sql` 与 `database/init_data.sql`；
2. 本 README 的表清单与口径说明；
3. `docs/04-DATABASE.md` 对应章节；
4. Qt 侧 Repository / Model 代码；
5. `docs/03-API.md` 涉及的消息字段。
