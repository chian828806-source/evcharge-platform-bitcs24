#include "pilerepository.h"

#include <QHash>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

QJsonArray PileRepository::statusSummary(int *total) const
{
    clearError();
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT status, COUNT(*) FROM charging_pile GROUP BY status"))) {
        m_lastError = query.lastError().text();
        return {};
    }
    QHash<QString, int> counts;
    int sum = 0;
    while (query.next()) {
        const int count = query.value(1).toInt();
        counts.insert(query.value(0).toString(), count);
        sum += count;
    }
    if (total) *total = sum;
    QJsonArray result;
    for (const QString &status : {QStringLiteral("AVAILABLE"), QStringLiteral("RESERVED"),
                                  QStringLiteral("CHARGING"), QStringLiteral("FAULT"),
                                  QStringLiteral("OFFLINE"), QStringLiteral("RESTARTING")}) {
        const int count = counts.value(status);
        result.append(QJsonObject{{QStringLiteral("status"), status},
                                  {QStringLiteral("count"), count},
                                  {QStringLiteral("ratio"), sum ? double(count) / sum : 0.0}});
    }
    return result;
}

QJsonArray PileRepository::list(qint64 stationId) const
{
    clearError();
    QSqlQuery query(m_database);
    QString sql = QStringLiteral("SELECT p.id, p.pile_no, s.id, s.name, p.type, p.power_kw, p.status, "
                                 "p.total_charge_count, p.total_charge_minutes FROM charging_pile p "
                                 "JOIN charging_station s ON s.id = p.station_id");
    if (stationId > 0) sql += QStringLiteral(" WHERE s.id = :stationId");
    query.prepare(sql + QStringLiteral(" ORDER BY s.station_no, p.pile_no"));
    if (stationId > 0) query.bindValue(QStringLiteral(":stationId"), stationId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return {};
    }
    QJsonArray result;
    while (query.next()) {
        result.append(QJsonObject{{QStringLiteral("pileId"), query.value(0).toLongLong()},
                                  {QStringLiteral("pileNo"), query.value(1).toString()},
                                  {QStringLiteral("stationId"), query.value(2).toLongLong()},
                                  {QStringLiteral("stationName"), query.value(3).toString()},
                                  {QStringLiteral("type"), query.value(4).toString()},
                                  {QStringLiteral("powerKw"), query.value(5).toDouble()},
                                  {QStringLiteral("status"), query.value(6).toString()},
                                  {QStringLiteral("totalChargeCount"), query.value(7).toInt()},
                                  {QStringLiteral("totalChargeMinutes"), query.value(8).toInt()}});
    }
    return result;
}

QString PileRepository::status(qint64 pileId, bool *found) const
{
    clearError();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), pileId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        if (found) *found = false;
        return {};
    }
    const bool exists = query.next();
    if (found) *found = exists;
    return exists ? query.value(0).toString() : QString();
}

bool PileRepository::compareAndSetStatus(qint64 pileId, const QString &before,
                                          const QString &after, const QString &now) const
{
    clearError();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE charging_pile SET status = :after, updated_at = :now "
                                 "WHERE id = :id AND status = :before"));
    query.bindValue(QStringLiteral(":after"), after);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":id"), pileId);
    query.bindValue(QStringLiteral(":before"), before);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

bool PileRepository::createForStation(qint64 stationId, int count, const QString &now) const
{
    clearError();
    for (int number = 1; number <= count; ++number) {
        QSqlQuery query(m_database);
        query.prepare(QStringLiteral("INSERT INTO charging_pile(station_id, pile_no, type, power_kw, status, created_at, updated_at) "
                                     "VALUES(:stationId, :number, :type, :power, 'OFFLINE', :now, :now)"));
        query.bindValue(QStringLiteral(":stationId"), stationId);
        query.bindValue(QStringLiteral(":number"), QStringLiteral("P%1").arg(number, 3, 10, QLatin1Char('0')));
        query.bindValue(QStringLiteral(":type"), number % 2 ? QStringLiteral("FAST") : QStringLiteral("SLOW"));
        query.bindValue(QStringLiteral(":power"), number % 2 ? 60.0 : 7.0);
        query.bindValue(QStringLiteral(":now"), now);
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            return false;
        }
    }
    return true;
}

bool PileRepository::deviceHello(qint64 pileId, const QString &pileNo, double *ratedPower,
                                 QString *status) const
{
    clearError(); QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT power_kw,status FROM charging_pile WHERE id=:id AND pile_no=:pileNo"));
    query.bindValue(QStringLiteral(":id"), pileId); query.bindValue(QStringLiteral(":pileNo"), pileNo);
    if (!query.exec() || !query.next()) { m_lastError=query.lastError().text(); return false; }
    if (ratedPower) *ratedPower = query.value(0).toDouble();
    if (status) *status = query.value(1).toString();
    return true;
}
bool PileRepository::updateHeartbeat(qint64 pileId, const QString &now) const
{ clearError(); QSqlQuery query(m_database); query.prepare(QStringLiteral("UPDATE charging_pile SET last_heartbeat_at=:now WHERE id=:id")); query.bindValue(QStringLiteral(":now"),now);query.bindValue(QStringLiteral(":id"),pileId); if(!query.exec()){m_lastError=query.lastError().text();return false;}return query.numRowsAffected()==1; }
std::optional<PileRepository::DeviceState> PileRepository::deviceState(qint64 pileId) const
{ clearError(); QSqlQuery query(m_database);query.prepare(QStringLiteral("SELECT status,current_order_id FROM charging_pile WHERE id=:id"));query.bindValue(QStringLiteral(":id"),pileId);if(!query.exec()){m_lastError=query.lastError().text();return std::nullopt;}if(!query.next())return std::nullopt;return DeviceState{query.value(0).toString(),query.value(1).toLongLong()}; }
bool PileRepository::transitionDeviceState(qint64 pileId,const QString &before,const QString &after,const QString &now,qint64 expectedOrderId) const
{ clearError();QSqlQuery query(m_database);QString sql=QStringLiteral("UPDATE charging_pile SET status=:after, current_order_id=NULL, updated_at=:now WHERE id=:id AND status=:before");if(expectedOrderId>0)sql+=QStringLiteral(" AND current_order_id=:orderId");query.prepare(sql);query.bindValue(QStringLiteral(":after"),after);query.bindValue(QStringLiteral(":now"),now);query.bindValue(QStringLiteral(":id"),pileId);query.bindValue(QStringLiteral(":before"),before);if(expectedOrderId>0)query.bindValue(QStringLiteral(":orderId"),expectedOrderId);if(!query.exec()){m_lastError=query.lastError().text();return false;}return query.numRowsAffected()==1; }
