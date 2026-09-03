/*
 * 功能：实现订单和电桩预约相关的参数化SQL。
 */
#include "orderrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QHash>
#include <QVariant>

namespace {

QString orderSelectSql(const QString &whereClause)
{
    return QStringLiteral(
        "SELECT o.id, o.order_no, o.user_id, o.station_id, s.name, "
        "o.pile_id, p.pile_no, p.power_kw, o.status, o.price_fen_per_kwh, "
        "o.service_fee_fen_per_kwh, o.start_at, o.end_at, "
        "o.charge_minutes, o.energy_kwh, o.amount_fen, o.created_at "
        "FROM charging_order o "
        "LEFT JOIN charging_station s ON s.id = o.station_id "
        "LEFT JOIN charging_pile p ON p.id = o.pile_id "
        "%1").arg(whereClause);
}

}

std::optional<ChargingOrderInfo> OrderRepository::findActiveByUser(
    QSqlDatabase &database, qint64 userId, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(orderSelectSql(QStringLiteral(
        "WHERE o.user_id = :userId "
        "AND o.status IN ('CREATED', 'CHARGING', 'PENDING_PAYMENT') "
        "ORDER BY o.created_at DESC, o.id DESC LIMIT 1")));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingOrderInfo>(mapOrder(query))
                        : std::nullopt;
}

std::optional<ChargingOrderInfo> OrderRepository::findByIdForUser(
    QSqlDatabase &database, qint64 orderId, qint64 userId,
    QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(orderSelectSql(
        QStringLiteral("WHERE o.id = :orderId AND o.user_id = :userId")));
    query.bindValue(QStringLiteral(":orderId"), orderId);
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    return query.next() ? std::optional<ChargingOrderInfo>(mapOrder(query))
                        : std::nullopt;
}

QList<ChargingOrderInfo> OrderRepository::listByUser(
    QSqlDatabase &database, qint64 userId, const QString &status, int limit, int offset,
    QString *errorMessage) const
{
    const bool filterStatus = !status.isEmpty();
    QSqlQuery query(database);
    query.prepare(orderSelectSql(QStringLiteral(
        "WHERE o.user_id = :userId %1 "
        "ORDER BY o.created_at DESC, o.id DESC LIMIT :limit OFFSET :offset")
        .arg(filterStatus ? QStringLiteral("AND o.status = :status") : QString())));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (filterStatus) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    query.bindValue(QStringLiteral(":limit"), limit);
    query.bindValue(QStringLiteral(":offset"), offset);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return {};
    }
    QList<ChargingOrderInfo> orders;
    while (query.next()) {
        orders.append(mapOrder(query));
    }
    return orders;
}

qint64 OrderRepository::countByUser(QSqlDatabase &database, qint64 userId,
                                    const QString &status,
                                    QString *errorMessage) const
{
    const bool filterStatus = !status.isEmpty();
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM charging_order WHERE user_id = :userId %1")
        .arg(filterStatus ? QStringLiteral("AND status = :status") : QString()));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (filterStatus) {
        query.bindValue(QStringLiteral(":status"), status);
    }
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }
    return query.value(0).toLongLong();
}

std::optional<OrderCreateTarget> OrderRepository::findCreateTarget(
    QSqlDatabase &database, qint64 pileId, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT p.id, p.station_id, p.status, s.status, "
        "s.price_fen_per_kwh, s.service_fee_fen_per_kwh "
        "FROM charging_pile p "
        "LEFT JOIN charging_station s ON s.id = p.station_id "
        "WHERE p.id = :pileId"));
    query.bindValue(QStringLiteral(":pileId"), pileId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return OrderCreateTarget{
        query.value(0).toLongLong(), query.value(1).toLongLong(),
        query.value(2).toString(), query.value(3).toString(),
        query.value(4).toLongLong(), query.value(5).toLongLong()
    };
}

bool OrderRepository::insertCreated(QSqlDatabase &database,
                                    const ChargingOrderInfo &order,
                                    qint64 *orderId, QString *errorMessage) const
{
    if (!orderId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("order ID output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO charging_order "
        "(order_no, user_id, station_id, pile_id, status, price_fen_per_kwh, "
        "service_fee_fen_per_kwh, created_at, updated_at) "
        "VALUES (:orderNo, :userId, :stationId, :pileId, 'CREATED', :price, "
        ":serviceFee, :now, :now)"));
    query.bindValue(QStringLiteral(":orderNo"), order.orderNo);
    query.bindValue(QStringLiteral(":userId"), order.userId);
    query.bindValue(QStringLiteral(":stationId"), order.stationId);
    query.bindValue(QStringLiteral(":pileId"), order.pileId);
    query.bindValue(QStringLiteral(":price"), order.priceFenPerKwh);
    query.bindValue(QStringLiteral(":serviceFee"), order.serviceFeeFenPerKwh);
    query.bindValue(QStringLiteral(":now"), order.createdAt);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *orderId = query.lastInsertId().toLongLong();
    return *orderId > 0;
}

bool OrderRepository::reservePile(QSqlDatabase &database, qint64 pileId,
                                  qint64 orderId, const QString &now,
                                  bool *reserved, QString *errorMessage) const
{
    if (!reserved) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("reservation output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_pile SET status = 'RESERVED', current_order_id = :orderId, "
        "updated_at = :now WHERE id = :pileId AND status = 'AVAILABLE'"));
    query.bindValue(QStringLiteral(":orderId"), orderId);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":pileId"), pileId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *reserved = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::startOrder(QSqlDatabase &database, qint64 orderId,
                                 const QString &now, bool *started,
                                 QString *errorMessage) const
{
    if (!started) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("start output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_order SET status = 'CHARGING', start_at = :now, "
        "updated_at = :now WHERE id = :orderId AND status = 'CREATED'"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *started = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::stopOrder(QSqlDatabase &database, qint64 orderId,
                                const QString &now, int chargeMinutes,
                                double energyKwh, qint64 amountFen,
                                bool *stopped, QString *errorMessage) const
{
    if (!stopped) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("stop output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_order SET status = 'PENDING_PAYMENT', end_at = :now, "
        "charge_minutes = :minutes, energy_kwh = :energy, amount_fen = :amount, "
        "updated_at = :now WHERE id = :orderId AND status = 'CHARGING'"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":minutes"), chargeMinutes);
    query.bindValue(QStringLiteral(":energy"), energyKwh);
    query.bindValue(QStringLiteral(":amount"), amountFen);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *stopped = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::cancelOrder(QSqlDatabase &database, qint64 orderId,
                                  const QString &now, const QString &reason,
                                  bool *cancelled, QString *errorMessage) const
{
    if (!cancelled) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("cancel output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_order SET status = 'CANCELLED', cancelled_at = :now, "
        "cancel_reason = :reason, updated_at = :now "
        "WHERE id = :orderId AND status = 'CREATED'"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":reason"), reason);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *cancelled = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::completeOrder(QSqlDatabase &database, qint64 orderId,
                                    const QString &now, bool *completed,
                                    QString *errorMessage) const
{
    if (!completed) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("completion output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_order SET status = 'COMPLETED', paid_at = :now, "
        "updated_at = :now WHERE id = :orderId AND status = 'PENDING_PAYMENT'"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *completed = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::startPile(QSqlDatabase &database, qint64 pileId,
                                qint64 orderId, const QString &now,
                                bool *started, QString *errorMessage) const
{
    if (!started) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("pile start output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_pile SET status = 'CHARGING', updated_at = :now "
        "WHERE id = :pileId AND status = 'RESERVED' AND current_order_id = :orderId"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":pileId"), pileId);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *started = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::releasePile(QSqlDatabase &database, qint64 pileId,
                                  qint64 orderId, const QString &expectedStatus,
                                  const QString &now, bool *released,
                                  QString *errorMessage) const
{
    if (!released) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("release output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_pile SET status = 'AVAILABLE', current_order_id = NULL, "
        "updated_at = :now WHERE id = :pileId AND status = :expectedStatus "
        "AND current_order_id = :orderId"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":pileId"), pileId);
    query.bindValue(QStringLiteral(":expectedStatus"), expectedStatus);
    query.bindValue(QStringLiteral(":orderId"), orderId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *released = query.numRowsAffected() == 1;
    return true;
}

bool OrderRepository::addPileStatistics(QSqlDatabase &database, qint64 pileId,
                                        int chargeMinutes, double energyKwh,
                                        const QString &now,
                                        QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE charging_pile SET total_charge_count = total_charge_count + 1, "
        "total_charge_minutes = total_charge_minutes + :minutes, "
        "total_energy_kwh = total_energy_kwh + :energy, updated_at = :now "
        "WHERE id = :pileId"));
    query.bindValue(QStringLiteral(":minutes"), chargeMinutes);
    query.bindValue(QStringLiteral(":energy"), energyKwh);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":pileId"), pileId);
    if (query.exec() && query.numRowsAffected() == 1) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

QJsonObject OrderRepository::revenueSummary(QSqlDatabase &database, const QDate &today,
                                             QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT COALESCE(SUM(CASE WHEN paid_at>=:dayStart AND paid_at<:dayEnd THEN amount_fen ELSE 0 END),0),"
                                 "COALESCE(SUM(CASE WHEN paid_at>=:monthStart AND paid_at<:dayEnd THEN amount_fen ELSE 0 END),0),"
                                 "COALESCE(SUM(amount_fen),0) FROM charging_order WHERE status='COMPLETED'"));
    query.bindValue(QStringLiteral(":dayStart"), today.toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    query.bindValue(QStringLiteral(":dayEnd"), today.addDays(1).toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    query.bindValue(QStringLiteral(":monthStart"), QDate(today.year(), today.month(), 1).toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    if (!query.exec() || !query.next()) { if (errorMessage) *errorMessage = query.lastError().text(); return {}; }
    return {{QStringLiteral("todayRevenueFen"), query.value(0).toLongLong()},
            {QStringLiteral("monthRevenueFen"), query.value(1).toLongLong()},
            {QStringLiteral("totalRevenueFen"), query.value(2).toLongLong()}};
}

QJsonArray OrderRepository::revenueTrend(QSqlDatabase &database, const QDate &firstDate,
                                          int days, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT substr(paid_at,1,10), COALESCE(SUM(amount_fen),0), "
                                 "COALESCE(SUM(energy_kwh),0), COUNT(*) FROM charging_order "
                                 "WHERE status='COMPLETED' AND paid_at>=:start GROUP BY substr(paid_at,1,10)"));
    query.bindValue(QStringLiteral(":start"), firstDate.toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    if (!query.exec()) { if (errorMessage) *errorMessage = query.lastError().text(); return {}; }
    QHash<QString, QJsonObject> values;
    while (query.next()) {
        values.insert(query.value(0).toString(),
                      {{QStringLiteral("revenueFen"), query.value(1).toLongLong()},
                       {QStringLiteral("energyKwh"), query.value(2).toDouble()},
                       {QStringLiteral("orderCount"), query.value(3).toInt()}});
    }
    QJsonArray result;
    for (int i = 0; i < days; ++i) {
        const QString date = firstDate.addDays(i).toString(Qt::ISODate);
        QJsonObject point = values.value(date,
            {{QStringLiteral("revenueFen"), 0}, {QStringLiteral("energyKwh"), 0.0},
             {QStringLiteral("orderCount"), 0}});
        point.insert(QStringLiteral("date"), date);
        result.append(point);
    }
    return result;
}

double OrderRepository::todayCompletedEnergyKwh(QSqlDatabase &database, const QDate &today,
                                                 QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT COALESCE(SUM(energy_kwh),0) FROM charging_order "
                                 "WHERE status='COMPLETED' AND paid_at>=:dayStart AND paid_at<:dayEnd"));
    query.bindValue(QStringLiteral(":dayStart"), today.toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    query.bindValue(QStringLiteral(":dayEnd"), today.addDays(1).toString(QStringLiteral("yyyy-MM-dd 00:00:00")));
    if (!query.exec() || !query.next()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return 0.0;
    }
    return query.value(0).toDouble();
}

qint64 OrderRepository::completedOrderCount(QSqlDatabase &database, QString *errorMessage) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM charging_order WHERE status='COMPLETED'"))
        || !query.next()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return 0;
    }
    return query.value(0).toLongLong();
}

ChargingOrderInfo OrderRepository::mapOrder(const QSqlQuery &query)
{
    ChargingOrderInfo order;
    order.orderId = query.value(0).toLongLong();
    order.orderNo = query.value(1).toString();
    order.userId = query.value(2).toLongLong();
    order.stationId = query.value(3).toLongLong();
    order.stationName = query.value(4).toString();
    order.pileId = query.value(5).toLongLong();
    order.pileNo = query.value(6).toString();
    order.powerKw = query.value(7).toDouble();
    order.status = query.value(8).toString();
    order.priceFenPerKwh = query.value(9).toLongLong();
    order.serviceFeeFenPerKwh = query.value(10).toLongLong();
    order.startAt = query.value(11).toString();
    order.endAt = query.value(12).toString();
    order.chargeMinutes = query.value(13).toInt();
    order.energyKwh = query.value(14).toDouble();
    order.amountFen = query.value(15).toLongLong();
    order.createdAt = query.value(16).toString();
    return order;
}
