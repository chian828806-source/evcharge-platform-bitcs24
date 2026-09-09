# 用户会员、月卡与季卡实施计划

状态：拟实施；权益和支付规则确认后才能进入开发  
适用范围：用户钱包、会员卡购买、订单计费、管理端产品管理  
最后更新：2026-09-09

## 1. 目标

在“我的”页提供会员入口，用户可购买月卡或季卡，并在有效期内享受统一、可追溯的充电权益。

为减少概念混乱，首版建议定义为：

```text
会员 = 当前拥有有效月卡或季卡的用户状态
月卡 = 有效期 30 天的会员产品
季卡 = 有效期 90 天的会员产品
```

这意味着首版不再单独维护一个无法解释权益的“会员等级”；用户的会员状态由有效卡自动得出。

## 2. 实施前必须确认的规则

以下规则不应由开发人员自行猜测，须由组内确认并写入 SRS/API/数据库文档：

| 问题 | 推荐首版方案 |
| --- | --- |
| 卡的有效期 | 月卡 30 天、季卡 90 天，从购买成功时开始计算 |
| 购买支付方式 | 仅用钱包余额支付；余额不足则拒绝购买 |
| 叠加购买 | 允许续期：新卡从 `max(当前到期时间, 当前时间)` 起算 |
| 同时持有多张卡 | 不允许并行权益，统一续期到同一条有效会员记录 |
| 会员权益 | 首版只选一种可量化权益，例如“服务费按折扣比例收取” |
| 优惠券叠加 | 本功能未实现优惠券；将来必须明确优惠券和会员折扣的先后顺序 |
| 订单中到期 | 下单时快照权益；充电/结算过程中即使到期，也不改变该订单价格 |
| 退款/退卡 | 首版不支持退款；管理员如需补偿，另走人工记录或后续功能 |

不要在没有明确权益的情况下只做“月卡/季卡购买页面”。会员卡必须能清楚说明“花多少钱、有效多久、能省什么”。

## 3. 价格与计费口径

推荐将会员权益限定为“服务费折扣”，不直接修改基础电价。原因是现有订单已经区分电价与服务费，解释与对账都更清晰。

例如，若产品的 `service_fee_discount_bps = 8000`：

```text
有效服务费 = round(原服务费 × 8000 / 10000)
订单金额 = round(energyKwh × (电价 + 有效服务费))
```

`bps` 是万分比整数，避免用浮点数保存折扣造成金额误差。具体折扣值和卡售价由管理员配置，不在代码中写死。

订单创建时应把适用权益写入订单快照，例如：

```text
membership_card_id                  可空
member_service_fee_discount_bps     默认 10000
effective_service_fee_fen_per_kwh   实际快照
```

这样订单结算、历史订单和管理端对账都能还原当时的计费依据。

## 4. 拟新增数据模型

以下是建议模型，不是可直接执行的 schema；正式变更需要先更新 `docs/04-DATABASE.md` 并评审。

### 4.1 `membership_product`：管理员配置的卡产品

```text
id, product_no, name, card_type(MONTH/SEASON),
duration_days, sale_price_fen, service_fee_discount_bps,
status(ON_SALE/OFF_SALE), created_at, updated_at
```

### 4.2 `user_membership_card`：用户当前或历史持有记录

```text
id, user_id, product_id, card_type,
paid_amount_fen, service_fee_discount_bps,
effective_at, expires_at, status(ACTIVE/EXPIRED),
created_at, updated_at
```

产品价格和折扣必须在用户持卡记录中保留快照，管理员后来调价不能影响已购买权益。

### 4.3 `membership_purchase`：购买流水

```text
id, purchase_no, user_id, card_id, product_id,
amount_fen, balance_after_fen, created_at
```

它用于说明“余额为何减少”，不能只更新 `user.balance_fen` 而没有购买留痕。

## 5. 拟新增协议

| 消息 | 调用方 | 目的 |
| --- | --- | --- |
| `MEMBERSHIP_PRODUCT_LIST` | 用户端 | 查看在售月卡、季卡及权益说明 |
| `MEMBERSHIP_STATUS_GET` | 用户端 | 查看当前会员状态、到期时间和权益 |
| `MEMBERSHIP_PURCHASE` | 用户端 | 用余额购买或续期卡产品 |
| `ADMIN_MEMBERSHIP_PRODUCT_LIST` | 管理端 | 查看全部产品 |
| `ADMIN_MEMBERSHIP_PRODUCT_SAVE` | 管理端 | 新增、调价、上下架产品 |

`MEMBERSHIP_PURCHASE` 的成功响应至少应返回：更新后的 `balanceFen`、当前卡 `expiresAt`、产品快照和购买流水号。购买请求应以 `requestId + userId` 做幂等保护，防止网络重试扣两次钱。

## 6. 关键事务

购买成功必须在**同一个 SQLite 事务**内完成：

1. 读取正常用户、在售产品和当前有效卡；
2. 验证余额足够；
3. 扣减 `user.balance_fen`；
4. 新建或续期 `user_membership_card`；
5. 写入 `membership_purchase`；
6. 写入必要的操作日志；
7. 提交事务后才向客户端返回成功。

任一步失败必须回滚，不能出现“余额已扣、卡却没有生效”的状态。

## 7. 推荐代码分层

```text
qt-server/
├─ handlers/user/membershiphandler.*
├─ handlers/admin/adminmembershiphandler.*
├─ services/user/membershipservice.*
├─ services/admin/adminmembershipservice.*
├─ repositories/membershiprepository.*
└─ models/membershipinfo.*

qt-user/
└─ ui/                     # 钱包卡片、产品列表、我的会员页

qt-admin/
└─ ui/                     # 产品配置、上下架页面
```

## 8. 实施阶段

### 阶段 A：先锁定业务规则和 UI 文案

确定售价、有效期、折扣、续期、退款、优惠券叠加规则；完成用户端钱包入口、产品列表、购买确认页和会员状态页设计。

### 阶段 B：数据与用户端购买闭环

更新 schema、种子数据、数据库文档；实现产品列表、状态查询、购买事务、用户端展示和网络测试。

### 阶段 C：订单计费接入

在 `ORDER_CREATE` 时获取有效会员权益并写入订单快照；在 `ORDER_STOP`/`ORDER_SETTLE` 按快照计费；补充正常、到期、余额不足和续期的测试。

### 阶段 D：管理端配置与联调

实现管理员产品新增、调价、上下架；验证下架产品不允许新购、已购卡不受影响；完成用户端与管理端全链路演示。

## 9. 验收清单

- 用户能看见月卡/季卡价格、权益和到期时间；
- 余额足够时购买一次只扣一次钱，并生成可查询流水；
- 余额不足、冻结用户、下架产品均不能购买；
- 续期后到期时间计算正确；
- 有效会员订单按权益快照结算，到期后新订单恢复普通计费；
- 管理员修改产品不会改写用户已购买卡和历史订单；
- 所有金额数据库中均以“分”保存。
