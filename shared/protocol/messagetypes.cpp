/*
 * 功能：构造消息和大屏主题注册表。
 * 实现：函数内静态集合只初始化一次，后续调用只返回常量引用。
 */
#include "messagetypes.h"

namespace MessageTypes {

const QSet<QString> &tcpTypes()
{
    // Dispatcher使用集合快速拒绝未登记的消息类型。
    static const QSet<QString> types = {
        UserLogin, UserProfileGet, UserProfileUpdate, UserAvatarUpload, UserAvatarGet,
        UserRecharge, UserOrderList, StationListNearby, StationDetailGet,
        MapGeocode, MapRoutePlan, OrderActiveCheck, OrderCreate, OrderCancel, OrderStart,
        OrderStop, OrderSettle, AdminLogin, AdminRevenueSummary,
        AdminRevenueTrend, AdminPileStatusSummary, AdminPileList,
        AdminPileRestart, AdminStationList, AdminStationCreate, AdminUserList,
        AdminUserFreeze, AdminUserUnfreeze, PredictionList,
        PredictionRecommendation, PredictionWarning
    };
    return types;
}

const QSet<QString> &dashboardTopics()
{
    // WebSocket服务只允许文档明确列出的展示主题。
    static const QSet<QString> topics = {
        QStringLiteral("summary"),
        QStringLiteral("pileStatus"),
        QStringLiteral("revenueTrend"),
        QStringLiteral("prediction")
    };
    return topics;
}

}
