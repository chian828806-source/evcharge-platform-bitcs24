# Data generator (B)

根据一期业务实体可重复生成站点、电桩、订单和会话 CSV，并以固定随机种子注入缺失、重复、
数值、逻辑、枚举和引用异常。生成器不得改动 `database/evcharge.db`；输出必须满足 Raw
Contract 并记录种子、行数和注入统计。
