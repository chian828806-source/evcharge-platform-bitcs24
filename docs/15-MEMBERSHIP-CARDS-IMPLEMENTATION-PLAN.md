# 用户会员、月卡与季卡实施说明

状态：用户端购买闭环已实现，服务端产品读取、事务扣款和订单价格快照已接通。

## 已确定规则

- 月卡：648 元，购买后有效 30 天。
- 季卡：999 元，购买后有效 90 天。
- 两种卡均为 VIP，服务费统一按 8 折计算；基础电价不打折。
- 续期从 `max(当前到期时间, 当前时间)` 起算。
- 购买只能使用钱包余额，余额不足或用户被冻结时拒绝购买。
- 登录和资料查询会按 `membership_expires_at` 自动刷新会员状态；过期后清零剩余天数并恢复原价。

## 数据库

`user` 表增加 `is_member`（SQLite `INTEGER`，0/1）、`membership_remaining_days`、
`membership_expires_at` 和 `membership_discount_bps`。产品存放在
`membership_product`，购买流水存放在 `membership_purchase`。余额扣减、会员更新和流水写入在同一事务完成。

## Socket 接口

- `MEMBERSHIP_PRODUCT_LIST`：查询在售产品。
- `MEMBERSHIP_STATUS_GET`：返回用户资料中的 VIP 状态、剩余天数和到期时间。
- `MEMBERSHIP_PURCHASE`：提交 `productNo`（`VIP-MONTH` 或 `VIP-SEASON`），成功响应返回更新后的用户资料。

用户端会员卡名称、价格、期限和折扣优先读取 `MEMBERSHIP_PRODUCT_LIST`，服务端不可用时仅使用与种子数据一致的展示兜底值。

## 订单计费

创建订单时若 `user.is_member = 1`，服务费快照为：

```text
effectiveServiceFee = round(originalServiceFee * membershipDiscountBps / 10000)
```

当前种子产品的 `membershipDiscountBps` 为 `8000`。订单保存快照后，会员到期不会改变历史订单金额。
