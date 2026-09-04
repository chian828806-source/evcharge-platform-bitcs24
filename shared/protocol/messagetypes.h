/*
 * 功能：声明TCP业务消息、WebSocket消息和大屏订阅主题。
 * 用法：客户端发送、服务端注册路由时都引用这些常量，避免手写字符串。
 */
#pragma once

#include <QSet>
#include <QString>

// inline变量允许头文件被多个工程引用而不产生重复定义。
namespace MessageTypes {
inline const QString UserLogin = QStringLiteral("USER_LOGIN");
inline const QString UserProfileGet = QStringLiteral("USER_PROFILE_GET");
inline const QString UserProfileUpdate = QStringLiteral("USER_PROFILE_UPDATE");
inline const QString UserAvatarUpload = QStringLiteral("USER_AVATAR_UPLOAD");
inline const QString UserRecharge = QStringLiteral("USER_RECHARGE");
inline const QString UserOrderList = QStringLiteral("USER_ORDER_LIST");

inline const QString StationListNearby = QStringLiteral("STATION_LIST_NEARBY");
inline const QString StationDetailGet = QStringLiteral("STATION_DETAIL_GET");
inline const QString MapGeocode = QStringLiteral("MAP_GEOCODE");
inline const QString MapRoutePlan = QStringLiteral("MAP_ROUTE_PLAN");

inline const QString OrderActiveCheck = QStringLiteral("ORDER_ACTIVE_CHECK");
inline const QString OrderCreate = QStringLiteral("ORDER_CREATE");
inline const QString OrderCancel = QStringLiteral("ORDER_CANCEL");
inline const QString OrderStart = QStringLiteral("ORDER_START");
inline const QString OrderStop = QStringLiteral("ORDER_STOP");
inline const QString OrderSettle = QStringLiteral("ORDER_SETTLE");

inline const QString AdminLogin = QStringLiteral("ADMIN_LOGIN");
inline const QString AdminRevenueSummary = QStringLiteral("ADMIN_REVENUE_SUMMARY");
inline const QString AdminRevenueTrend = QStringLiteral("ADMIN_REVENUE_TREND");
inline const QString AdminPileStatusSummary = QStringLiteral("ADMIN_PILE_STATUS_SUMMARY");
inline const QString AdminPileList = QStringLiteral("ADMIN_PILE_LIST");
inline const QString AdminPileRestart = QStringLiteral("ADMIN_PILE_RESTART");
inline const QString AdminStationList = QStringLiteral("ADMIN_STATION_LIST");
inline const QString AdminStationCreate = QStringLiteral("ADMIN_STATION_CREATE");
inline const QString AdminUserList = QStringLiteral("ADMIN_USER_LIST");
inline const QString AdminUserFreeze = QStringLiteral("ADMIN_USER_FREEZE");
inline const QString AdminUserUnfreeze = QStringLiteral("ADMIN_USER_UNFREEZE");

inline const QString PredictionList = QStringLiteral("PREDICTION_LIST");
inline const QString PredictionRecommendation = QStringLiteral("PREDICTION_RECOMMENDATION");
inline const QString PredictionWarning = QStringLiteral("PREDICTION_WARNING");
inline const QString PredictionImport = QStringLiteral("PREDICTION_IMPORT");

inline const QString DashboardSubscribe = QStringLiteral("DASHBOARD_SUBSCRIBE");
inline const QString DashboardUpdate = QStringLiteral("DASHBOARD_UPDATE");

// 返回文档中31种TCP消息的只读集合。
const QSet<QString> &tcpTypes();
// 返回大屏允许订阅的4个主题。
const QSet<QString> &dashboardTopics();
}
