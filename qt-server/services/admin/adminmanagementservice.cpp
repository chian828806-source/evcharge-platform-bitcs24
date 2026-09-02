#include "adminmanagementservice.h"

#include "shared/protocol/errorcodes.h"

#include <QJsonArray>
#include <QSqlError>
#include <QSqlQuery>
#include <QDateTime>
#include <QTimer>
#include <utility>

AdminManagementService::AdminManagementService(QSqlDatabase database,
                                               QObject *parent)
    : QObject(parent), m_database(std::move(database))
{
}

ResponseMessage AdminManagementService::restartPile(const RequestMessage &request,
                                                     qint64 adminId) const
{
    const qint64 pileId = request.payload.value(QStringLiteral("pileId")).toInteger(0);
    QSqlQuery find(m_database);
    find.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id = :id"));
    find.bindValue(QStringLiteral(":id"), pileId);
    if (!find.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      find.lastError().text());
    }
    if (!find.next()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::PileNotFound,
                                      QStringLiteral("charging pile not found"));
    }
    const QString previousStatus = find.value(0).toString();
    if (previousStatus == QStringLiteral("RESERVED")
        || previousStatus == QStringLiteral("CHARGING")
        || previousStatus == QStringLiteral("RESTARTING")) {
        return ResponseMessage::error(request.requestId, ErrorCodes::PileUnavailable,
                                      QStringLiteral("current pile status cannot be restarted"));
    }
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral("UPDATE charging_pile SET status='RESTARTING', updated_at=:now WHERE id=:id AND status=:status"));
    update.bindValue(QStringLiteral(":now"), now);
    update.bindValue(QStringLiteral(":id"), pileId);
    update.bindValue(QStringLiteral(":status"), previousStatus);
    QSqlQuery log(m_database);
    log.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, before_status, after_status, result, message, created_at) VALUES(:adminId, 'PILE_RESTART', 'PILE', :pileId, :before, 'RESTARTING', 'SUCCESS', '远程重启指令已发送', :now)"));
    log.bindValue(QStringLiteral(":adminId"), adminId);
    log.bindValue(QStringLiteral(":pileId"), pileId);
    log.bindValue(QStringLiteral(":before"), previousStatus);
    log.bindValue(QStringLiteral(":now"), now);
    if (!update.exec() || update.numRowsAffected() != 1 || !log.exec()
        || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }

    QTimer::singleShot(1500, this,
                       [this, pileId, previousStatus, adminId]() {
        QSqlDatabase database = m_database;
        const QString completedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!database.transaction()) {
            return;
        }
        QSqlQuery restore(database);
        restore.prepare(QStringLiteral("UPDATE charging_pile SET status=:status, updated_at=:now WHERE id=:id AND status='RESTARTING'"));
        restore.bindValue(QStringLiteral(":status"), previousStatus);
        restore.bindValue(QStringLiteral(":now"), completedAt);
        restore.bindValue(QStringLiteral(":id"), pileId);
        QSqlQuery completedLog(database);
        completedLog.prepare(QStringLiteral("INSERT INTO operation_log(admin_id, action, target_type, target_id, before_status, after_status, result, message, created_at) VALUES(:adminId, 'PILE_RESTART', 'PILE', :pileId, 'RESTARTING', :after, 'SUCCESS', '远程重启模拟完成', :now)"));
        completedLog.bindValue(QStringLiteral(":adminId"), adminId);
        completedLog.bindValue(QStringLiteral(":pileId"), pileId);
        completedLog.bindValue(QStringLiteral(":after"), previousStatus);
        completedLog.bindValue(QStringLiteral(":now"), completedAt);
        if (!restore.exec() || restore.numRowsAffected() != 1 || !completedLog.exec()) {
            database.rollback();
            return;
        }
        database.commit();
    });
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("pileId"), pileId},
        {QStringLiteral("status"), QStringLiteral("RESTARTING")},
        {QStringLiteral("restoreStatus"), previousStatus}
    });
}

ResponseMessage AdminManagementService::pileList(const RequestMessage &request) const
{
    const qint64 stationId = request.payload.value(QStringLiteral("stationId")).toInteger(0);
    QSqlQuery query(m_database);
    QString sql = QStringLiteral(
        "SELECT p.id, p.pile_no, s.id, s.name, p.type, p.power_kw, p.status, "
        "p.total_charge_count, p.total_charge_minutes "
        "FROM charging_pile p JOIN charging_station s ON s.id = p.station_id");
    if (stationId > 0) {
        sql += QStringLiteral(" WHERE s.id = :stationId");
    }
    sql += QStringLiteral(" ORDER BY s.station_no, p.pile_no");
    query.prepare(sql);
    if (stationId > 0) {
        query.bindValue(QStringLiteral(":stationId"), stationId);
    }
    if (!query.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QJsonArray piles;
    while (query.next()) {
        piles.append(QJsonObject{
            {QStringLiteral("pileId"), query.value(0).toLongLong()},
            {QStringLiteral("pileNo"), query.value(1).toString()},
            {QStringLiteral("stationId"), query.value(2).toLongLong()},
            {QStringLiteral("stationName"), query.value(3).toString()},
            {QStringLiteral("type"), query.value(4).toString()},
            {QStringLiteral("powerKw"), query.value(5).toDouble()},
            {QStringLiteral("status"), query.value(6).toString()},
            {QStringLiteral("totalChargeCount"), query.value(7).toInt()},
            {QStringLiteral("totalChargeMinutes"), query.value(8).toInt()}
        });
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("piles"), piles}});
}

ResponseMessage AdminManagementService::stationList(const RequestMessage &request) const
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.station_no, s.name, s.address, s.longitude, s.latitude, "
            "COUNT(p.id), COALESCE(SUM(CASE WHEN p.status <> 'OFFLINE' THEN 1 ELSE 0 END), 0) "
            "FROM charging_station s LEFT JOIN charging_pile p ON p.station_id=s.id "
            "GROUP BY s.id ORDER BY s.station_no"))) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QJsonArray stations;
    while (query.next()) {
        const int pileCount = query.value(6).toInt();
        stations.append(QJsonObject{
            {QStringLiteral("stationId"), query.value(0).toLongLong()},
            {QStringLiteral("stationNo"), query.value(1).toString()},
            {QStringLiteral("name"), query.value(2).toString()},
            {QStringLiteral("address"), query.value(3).toString()},
            {QStringLiteral("longitude"), query.value(4).toDouble()},
            {QStringLiteral("latitude"), query.value(5).toDouble()},
            {QStringLiteral("pileCount"), pileCount},
            {QStringLiteral("onlineRate"), pileCount > 0
                 ? query.value(7).toDouble() / pileCount : 0.0}
        });
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("stations"), stations}});
}

ResponseMessage AdminManagementService::createStation(const RequestMessage &request,
                                                       qint64 adminId) const
{
    const QString name = request.payload.value(QStringLiteral("name")).toString().trimmed();
    const QString address = request.payload.value(QStringLiteral("address")).toString().trimmed();
    const double longitude = request.payload.value(QStringLiteral("longitude")).toDouble(999.0);
    const double latitude = request.payload.value(QStringLiteral("latitude")).toDouble(999.0);
    const int pileCount = request.payload.value(QStringLiteral("pileCount")).toInt();
    const int priceFen = request.payload.value(QStringLiteral("priceFenPerKwh")).toInt(120);
    if (name.isEmpty() || address.isEmpty() || longitude < -180 || longitude > 180
        || latitude < -90 || latitude > 90 || pileCount < 1 || pileCount > 100
        || priceFen < 0) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("invalid station fields"));
    }
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString stationNo = QStringLiteral("ST%1")
        .arg(QDateTime::currentMSecsSinceEpoch());
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    QSqlQuery station(m_database);
    station.prepare(QStringLiteral("INSERT INTO charging_station(station_no,name,address,longitude,latitude,price_fen_per_kwh,service_fee_fen_per_kwh,status,created_at,updated_at) VALUES(:no,:name,:address,:longitude,:latitude,:price,0,'NORMAL',:now,:now)"));
    station.bindValue(QStringLiteral(":no"), stationNo);
    station.bindValue(QStringLiteral(":name"), name);
    station.bindValue(QStringLiteral(":address"), address);
    station.bindValue(QStringLiteral(":longitude"), longitude);
    station.bindValue(QStringLiteral(":latitude"), latitude);
    station.bindValue(QStringLiteral(":price"), priceFen);
    station.bindValue(QStringLiteral(":now"), now);
    if (!station.exec()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      station.lastError().text());
    }
    const qint64 stationId = station.lastInsertId().toLongLong();
    for (int number = 1; number <= pileCount; ++number) {
        QSqlQuery pile(m_database);
        pile.prepare(QStringLiteral("INSERT INTO charging_pile(station_id,pile_no,type,power_kw,status,created_at,updated_at) VALUES(:stationId,:pileNo,:type,:power,'AVAILABLE',:now,:now)"));
        pile.bindValue(QStringLiteral(":stationId"), stationId);
        pile.bindValue(QStringLiteral(":pileNo"), QStringLiteral("P%1").arg(number, 3, 10, QLatin1Char('0')));
        pile.bindValue(QStringLiteral(":type"), number % 2 ? QStringLiteral("FAST") : QStringLiteral("SLOW"));
        pile.bindValue(QStringLiteral(":power"), number % 2 ? 60.0 : 7.0);
        pile.bindValue(QStringLiteral(":now"), now);
        if (!pile.exec()) {
            m_database.rollback();
            return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                          pile.lastError().text());
        }
    }
    QSqlQuery log(m_database);
    log.prepare(QStringLiteral("INSERT INTO operation_log(admin_id,action,target_type,target_id,result,message,created_at) VALUES(:adminId,'STATION_CREATE','STATION',:stationId,'SUCCESS',:message,:now)"));
    log.bindValue(QStringLiteral(":adminId"), adminId);
    log.bindValue(QStringLiteral(":stationId"), stationId);
    log.bindValue(QStringLiteral(":message"), QStringLiteral("新增充电站并生成 %1 个模拟电桩").arg(pileCount));
    log.bindValue(QStringLiteral(":now"), now);
    if (!log.exec() || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      log.lastError().text());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("stationId"), stationId},
        {QStringLiteral("stationNo"), stationNo},
        {QStringLiteral("pileCount"), pileCount}
    });
}

ResponseMessage AdminManagementService::userList(const RequestMessage &request) const
{
    QString keyword = request.payload.value(QStringLiteral("phoneKeyword")).toString().trimmed();
    keyword.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    keyword.replace(QLatin1Char('%'), QStringLiteral("\\%"));
    keyword.replace(QLatin1Char('_'), QStringLiteral("\\_"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, balance_fen, created_at, status "
        "FROM user WHERE phone LIKE :keyword ESCAPE '\\' ORDER BY id"));
    query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%") + keyword + QStringLiteral("%"));
    if (!query.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      query.lastError().text());
    }
    QJsonArray users;
    while (query.next()) {
        users.append(QJsonObject{
            {QStringLiteral("userId"), query.value(0).toLongLong()},
            {QStringLiteral("phone"), query.value(1).toString()},
            {QStringLiteral("nickname"), query.value(2).toString()},
            {QStringLiteral("balanceFen"), query.value(3).toLongLong()},
            {QStringLiteral("createdAt"), query.value(4).toString()},
            {QStringLiteral("status"), query.value(5).toString()}
        });
    }
    return ResponseMessage::success(request.requestId,
                                    {{QStringLiteral("users"), users}});
}

ResponseMessage AdminManagementService::setUserFrozen(const RequestMessage &request,
                                                       qint64 adminId,
                                                       bool frozen) const
{
    const qint64 userId = request.payload.value(QStringLiteral("userId")).toInteger(0);
    if (userId <= 0) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidSocketMessage,
                                      QStringLiteral("userId must be a positive integer"));
    }
    QSqlQuery find(m_database);
    find.prepare(QStringLiteral("SELECT phone, status FROM user WHERE id=:userId"));
    find.bindValue(QStringLiteral(":userId"), userId);
    if (!find.exec()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      find.lastError().text());
    }
    if (!find.next()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::InvalidPhone,
                                      QStringLiteral("user not found"));
    }
    const QString phone = find.value(0).toString();
    const QString beforeStatus = find.value(1).toString();
    const QString afterStatus = frozen ? QStringLiteral("FROZEN")
                                       : QStringLiteral("NORMAL");
    if (beforeStatus == afterStatus) {
        return ResponseMessage::success(request.requestId, {
            {QStringLiteral("userId"), userId},
            {QStringLiteral("status"), afterStatus},
            {QStringLiteral("changed"), false}
        });
    }

    const QString now = QDateTime::currentDateTime()
                            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!m_database.transaction()) {
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      m_database.lastError().text());
    }
    QSqlQuery update(m_database);
    update.prepare(QStringLiteral(
        "UPDATE user SET status=:after, updated_at=:now "
        "WHERE id=:userId AND status=:before"));
    update.bindValue(QStringLiteral(":after"), afterStatus);
    update.bindValue(QStringLiteral(":now"), now);
    update.bindValue(QStringLiteral(":userId"), userId);
    update.bindValue(QStringLiteral(":before"), beforeStatus);
    QSqlQuery log(m_database);
    log.prepare(QStringLiteral(
        "INSERT INTO operation_log(admin_id,action,target_type,target_id,"
        "before_status,after_status,result,message,created_at) "
        "VALUES(:adminId,:action,'USER',:userId,:before,:after,'SUCCESS',:message,:now)"));
    log.bindValue(QStringLiteral(":adminId"), adminId);
    log.bindValue(QStringLiteral(":action"), frozen
                      ? QStringLiteral("USER_FREEZE")
                      : QStringLiteral("USER_UNFREEZE"));
    log.bindValue(QStringLiteral(":userId"), userId);
    log.bindValue(QStringLiteral(":before"), beforeStatus);
    log.bindValue(QStringLiteral(":after"), afterStatus);
    log.bindValue(QStringLiteral(":message"),
                  (frozen ? QStringLiteral("冻结用户 ") : QStringLiteral("解冻用户 "))
                      + phone);
    log.bindValue(QStringLiteral(":now"), now);
    if (!update.exec() || update.numRowsAffected() != 1 || !log.exec()
        || !m_database.commit()) {
        m_database.rollback();
        return ResponseMessage::error(request.requestId, ErrorCodes::DatabaseError,
                                      update.lastError().text());
    }
    return ResponseMessage::success(request.requestId, {
        {QStringLiteral("userId"), userId},
        {QStringLiteral("status"), afterStatus},
        {QStringLiteral("changed"), true}
    });
}
