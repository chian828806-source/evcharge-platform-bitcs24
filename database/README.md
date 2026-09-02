# database/ — 数据库脚本

对齐 `docs/04-DATABASE.md` 新版契约（8 表 + 15 索引 + 事务规则）。

## 文件

| 文件 | 作用 |
| --- | --- |
| `schema.sql` | 建库脚本：8 张表 + 15 个索引，可重复执行（先 DROP 再 CREATE） |
| `init_data.sql` | 演示种子数据：满足 04 文档第 11 节全部初始化要求 |
| `evcharge.db` | 运行时生成的 SQLite 数据库文件（已被 .gitignore 忽略，不入库） |

## 快速开始

```bash
# 方式一：一行初始化到项目约定路径
mkdir -p database && sqlite3 database/evcharge.db ".read database/schema.sql" ".read database/init_data.sql"

# 方式二：分步执行
sqlite3 database/evcharge.db
sqlite> .read database/schema.sql
sqlite> .read database/init_data.sql
```

> 注意：`schema.sql` 会先 DROP 再 CREATE，**重复执行会清空全部业务数据**。

## 内置演示账号

| 角色 | 账号 | 密码 | 说明 |
| --- | --- | --- | --- |
| 管理员 | `admin` | `123456` | PBKDF2-HMAC-SHA256（`pbkdf2_sha256$iterations$salt_hex$hash_hex`） |
| 用户 | `13800000001` ~ `13800000005` | 任意 6 位（登录即注册） | 用户3 余额不足、用户4 已冻结 |

## 演示数据亮点（配合验收）

- **状态全覆盖**：12 桩含 `AVAILABLE/CHARGING/RESERVED/FAULT/OFFLINE`；
  桩4/桩11 为 FAULT，可演示远程重启（BR-007：RESERVED/CHARGING 桩拒绝重启）。
- **完整订单链**：18 单覆盖近 30 日（QChart/ECharts 趋势数据）、
  1 单 `CHARGING`（桩2）、1 单 `PENDING_PAYMENT`（订单15，用户3 余额不足）、
  1 单可取消 `CREATED`（订单17 ↔ 桩5 RESERVED）。
- **可对账钱包**：每个用户的 `balance_fen` 严格等于
  `充值合计 − COMPLETED 订单金额`，充值流水 `balance_after_fen` 按时序可推演。
- **预测三档**：每站各 1 条 `1h/6h/24h` 预测，含 `peak_level` 与 `mae/rmse`。
- **留痕完整**：操作日志含管理员登录、冻结、**解冻**、远程重启（含前后状态）、
  新增站点、订单状态变更、系统错误。

## 存储约定（详见 04 文档第 2 节）

- 金额一律存 **分**（INTEGER）；电量/功率/比率用 REAL
- 时间统一 TEXT，格式 `yyyy-MM-dd HH:mm:ss`
- 数据库 snake_case ↔ Socket 报文 camelCase（如 `balance_fen` ↔ `balanceFen`）
- 计费公式：`amount_fen = round(energy_kwh * (price_fen_per_kwh + service_fee_fen_per_kwh))`
- 管理员密码格式为 `pbkdf2_sha256$iterations$salt_hex$hash_hex`；后续登录 Service 为每个账号生成独立随机盐，以相同迭代次数重新派生并做常量时间比较。

## 表结构变更流程

`schema.sql` 是**公共契约**，改动必须走 04 文档第 12 节流程：
提 PR 说明变更点与影响面 → 组内评审 → 同步更新 04 文档 → 合并。
禁止在功能分支上私自改字段不通知。
