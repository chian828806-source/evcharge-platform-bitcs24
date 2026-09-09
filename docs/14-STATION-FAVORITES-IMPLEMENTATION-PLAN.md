# 用户收藏充电站实施计划

状态：已实施，前端、服务端、数据库、协议与自动化测试均已落地
适用范围：用户端站点列表/详情/个人中心、Qt Server 用户业务
最后更新：2026-09-09

## 1. 目标与范围

用户可以把常用充电站收藏起来，在“我的收藏”中快速查看、进入详情或导航。收藏是“用户与站点”的关系，不是复制一份站点数据。

首版包含：

- 在站点卡片和站点详情中收藏/取消收藏；
- 首页站点卡片显示收藏状态；
- 个人中心新增“我的收藏”图标入口；
- 收藏列表按最近收藏时间展示，可进入站点详情或导航。

首版不包含：收藏分组、备注、共享收藏夹、收藏电桩到具体 `pileId`。

## 2. 业务规则

1. 同一用户对同一站点最多收藏一次；
2. 只有已登录且状态为 `NORMAL` 的用户可以新增或取消收藏；
3. 被收藏站点若被管理员停用，仍保留在收藏列表中，但标记“已停用”，不可创建订单；
4. 用户只能读取和修改自己的收藏；
5. 取消收藏只删除关联关系，不删除 `charging_station` 站点本身；
6. 收藏不影响附近站点排序、推荐算法和站点可用桩数量。

## 3. 新增数据表

数据库变更按 `docs/04-DATABASE.md` 的公共契约流程落地。本功能新增一张关联表：

```sql
CREATE TABLE user_station_favorite (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL REFERENCES user(id),
    station_id INTEGER NOT NULL REFERENCES charging_station(id),
    created_at TEXT NOT NULL,
    UNIQUE(user_id, station_id)
);

CREATE INDEX idx_favorite_user_created
    ON user_station_favorite(user_id, created_at DESC);
```

`UNIQUE(user_id, station_id)` 是最终兜底：即使客户端重复点击或网络重试，也不会出现重复收藏。

## 4. 新增协议

消息名、字段和错误码已同步到 `docs/03-API.md` 与 `shared/protocol`。

| 消息 | 请求 | 成功响应 | 说明 |
| --- | --- | --- | --- |
| `USER_STATION_FAVORITE_TOGGLE` | `stationId: integer` | `stationId`、`isFavorite: boolean` | 若未收藏则收藏，已收藏则取消；首版便于一个按钮切换 |
| `USER_STATION_FAVORITE_LIST` | 可选 `longitude`、`latitude` | `stations: []` | 返回用户收藏的站点及可选距离 |

为避免列表上的收藏按钮逐项请求，同时扩展了已登录用户的站点响应：

```text
STATION_LIST_NEARBY / STATION_DETAIL_GET 中每个站点新增：
isFavorite: boolean
```

该字段仅由服务端根据当前 Session 的 `userId` 计算，不能由客户端自行声明。

## 5. 推荐服务端分层

```text
UserWindow
  -> SocketClient
     -> MessageDispatcher
        -> FavoriteHandler
           -> FavoriteService
              -> FavoriteRepository
                 -> SQLite user_station_favorite
```

- `FavoriteHandler`：检查 `stationId` 类型与用户 Session；
- `FavoriteService`：检查用户状态、站点是否存在，组织事务；
- `FavoriteRepository`：只执行查询、插入、删除，不做业务判断；
- `StationService`：在站点列表/详情序列化时补充 `isFavorite`。

## 6. 实施步骤

### 阶段 A：数据与后端

1. 更新 `database/schema.sql`、`docs/04-DATABASE.md` 和种子数据；
2. 创建 Favorite Repository、Service、Handler 并注册消息；
3. 实现切换收藏的事务与收藏列表查询；
4. 在附近列表和站点详情响应中补充 `isFavorite`；
5. 更新 `docs/03-API.md`、`shared/protocol` 和网络协议测试。

### 阶段 B：用户端

1. 站点卡片与详情页增加星标/爱心按钮；
2. 个人中心“常用功能”增加“我的收藏”入口；
3. 收藏列表显示名称、地址、状态、空闲桩和距离；
4. 收藏/取消后立即刷新当前卡片和收藏列表，无需重新登录。

### 阶段 C：联调与验收

1. 同一用户重复点击不会生成重复数据；
2. 两个用户的收藏互不影响；
3. 停用站点仍可看到，但不能下单；
4. 站点在附近列表、详情和收藏列表中的收藏状态一致；
5. 无效站点、Session 失效、冻结用户均返回可理解的错误。

## 7. 已确认决策

1. 收藏按钮采用星标，和“喜欢/点赞”语义区分；
2. 收藏列表默认按最近收藏时间排序，传入当前位置时同时展示距离；
3. 管理端首版不增加收藏人数统计，只预留后续扩展可能。

## 8. 实施结果

- 收藏按钮采用星标；列表默认按最近收藏时间倒序；管理端收藏统计不纳入首版；
- 新增 `user_station_favorite` 表及旧数据库幂等增量升级脚本；
- 新增 Favorite Repository、Service、Handler，并以普通用户权限注册两条消息；
- 附近站点、推荐站点和详情统一返回服务端计算的 `isFavorite`；
- 停用站点保留在收藏列表，仅收藏该站的用户可查看详情，客户端禁止下单；
- 用户端完成首页、详情、个人中心入口、收藏列表、刷新及错误/空数据状态；
- 自动化测试覆盖收藏切换、用户隔离、冻结用户、无效站点和停用站点边界。
