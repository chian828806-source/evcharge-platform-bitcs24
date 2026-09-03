/* 功能：把当前业务Repository结果转换为Dashboard既定四个topic的数据形状。 */
#include "dashboarddataservice.h"

#include "database/databasemanager.h"
#include "repositories/orderrepository.h"
#include "repositories/pilerepository.h"
#include "repositories/predictionrepository.h"
#include "services/prediction/predictionservice.h"
#include "shared/protocol/errorcodes.h"

#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>

namespace {

ServiceResult<QSqlDatabase> databaseFor(DatabaseManager *manager)
{
    QSqlDatabase database;
    QString error;
    if (!manager || !manager->database(&database, &error)) {
        return ServiceResult<QSqlDatabase>::failure(ErrorCodes::DatabaseError, error);
    }
    return ServiceResult<QSqlDatabase>::success(database);
}

}

DashboardDataService::DashboardDataService(DatabaseManager *databaseManager)
    : m_databaseManager(databaseManager),
      m_predictionRepository(new PredictionRepository),
      m_predictionService(new PredictionService(databaseManager, m_predictionRepository))
{
}

DashboardDataService::~DashboardDataService()
{
    delete m_predictionService;
    delete m_predictionRepository;
}

ServiceResult<QJsonObject> DashboardDataService::dataForTopic(const QString &topic) const
{
    if (topic == QStringLiteral("summary")) return summary();
    if (topic == QStringLiteral("pileStatus")) return pileStatus();
    if (topic == QStringLiteral("revenueTrend")) return revenueTrend();
    if (topic == QStringLiteral("prediction")) return prediction();
    return ServiceResult<QJsonObject>::failure(ErrorCodes::InvalidSocketMessage,
                                                QStringLiteral("unknown dashboard topic"));
}

ServiceResult<QJsonObject> DashboardDataService::summary() const
{
    const auto databaseResult = databaseFor(m_databaseManager);
    if (!databaseResult.ok) return ServiceResult<QJsonObject>::failure(databaseResult.code,
                                                                         databaseResult.message);
    QString error;
    OrderRepository orders;
    const QJsonObject revenue = orders.revenueSummary(databaseResult.value,
                                                       QDate::currentDate(), &error);
    if (!error.isEmpty()) return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
    const double energy = orders.todayCompletedEnergyKwh(databaseResult.value,
                                                         QDate::currentDate(), &error);
    if (!error.isEmpty()) return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
    const qint64 orderCount = orders.completedOrderCount(databaseResult.value, &error);
    if (!error.isEmpty()) return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);

    PileRepository piles(databaseResult.value);
    int total = 0;
    const QJsonArray statuses = piles.statusSummary(&total);
    if (!piles.lastError().isEmpty()) return ServiceResult<QJsonObject>::failure(
        ErrorCodes::DatabaseError, piles.lastError());
    int occupied = 0;
    for (const QJsonValue &value : statuses) {
        const QJsonObject status = value.toObject();
        const QString name = status.value(QStringLiteral("status")).toString();
        if (name == QStringLiteral("RESERVED") || name == QStringLiteral("CHARGING")) {
            occupied += status.value(QStringLiteral("count")).toInt();
        }
    }
    return ServiceResult<QJsonObject>::success({
        {QStringLiteral("todayEnergyKwh"), energy},
        {QStringLiteral("todayRevenueFen"), revenue.value(QStringLiteral("todayRevenueFen"))},
        {QStringLiteral("totalOrderCount"), orderCount},
        {QStringLiteral("stationLoad"), total > 0 ? static_cast<double>(occupied) / total : 0.0}
    });
}

ServiceResult<QJsonObject> DashboardDataService::pileStatus() const
{
    const auto databaseResult = databaseFor(m_databaseManager);
    if (!databaseResult.ok) return ServiceResult<QJsonObject>::failure(databaseResult.code,
                                                                         databaseResult.message);
    PileRepository piles(databaseResult.value);
    int total = 0;
    const QJsonArray statuses = piles.statusSummary(&total);
    if (!piles.lastError().isEmpty()) return ServiceResult<QJsonObject>::failure(
        ErrorCodes::DatabaseError, piles.lastError());
    QJsonObject counts;
    for (const QJsonValue &value : statuses) {
        const QJsonObject status = value.toObject();
        counts.insert(status.value(QStringLiteral("status")).toString(),
                      status.value(QStringLiteral("count")).toInt());
    }
    return ServiceResult<QJsonObject>::success({{QStringLiteral("counts"), counts}});
}

ServiceResult<QJsonObject> DashboardDataService::revenueTrend() const
{
    const auto databaseResult = databaseFor(m_databaseManager);
    if (!databaseResult.ok) return ServiceResult<QJsonObject>::failure(databaseResult.code,
                                                                         databaseResult.message);
    OrderRepository orders;
    QString error;
    const QDate today = QDate::currentDate();
    const QJsonArray week = orders.revenueTrend(databaseResult.value, today.addDays(-6), 7, &error);
    if (!error.isEmpty()) return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
    const QJsonArray month = orders.revenueTrend(databaseResult.value, today.addDays(-29), 30, &error);
    if (!error.isEmpty()) return ServiceResult<QJsonObject>::failure(ErrorCodes::DatabaseError, error);
    return ServiceResult<QJsonObject>::success({
        {QStringLiteral("ranges"), QJsonObject{
            {QStringLiteral("7d"), QJsonObject{{QStringLiteral("range"), QStringLiteral("7d")},
                                                  {QStringLiteral("items"), week}}},
            {QStringLiteral("30d"), QJsonObject{{QStringLiteral("range"), QStringLiteral("30d")},
                                                   {QStringLiteral("items"), month}}}
        }}
    });
}

ServiceResult<QJsonObject> DashboardDataService::prediction() const
{
    const auto result = m_predictionService->list(0, {}, 100);
    if (!result.ok) return ServiceResult<QJsonObject>::failure(result.code, result.message);
    const QJsonArray items = result.value;
    const QString generatedAt = items.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
        : items.first().toObject().value(QStringLiteral("generatedAt")).toString();
    return ServiceResult<QJsonObject>::success({{QStringLiteral("generatedAt"), generatedAt},
                                                {QStringLiteral("items"), items}});
}
