/*
 * 功能：实现用户资料查询、自动注册和资料更新SQL。
 */
#include "userrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDateTime>
#include <QtMath>
#include <QUuid>

namespace {

QString userSelectSql(QSqlDatabase &database)
{
    QSqlQuery columns(database);
    bool hasMembershipColumns = false;
    if (columns.exec(QStringLiteral("PRAGMA table_info(user)"))) {
        while (columns.next()) {
            const QString name = columns.value(1).toString();
            if (name == QStringLiteral("is_member")) {
                hasMembershipColumns = true;
                break;
            }
        }
    }
    const QString membership = hasMembershipColumns
        ? QStringLiteral("is_member, membership_remaining_days, membership_expires_at, membership_discount_bps")
        : QStringLiteral("0 AS is_member, 0 AS membership_remaining_days, NULL AS membership_expires_at, 10000 AS membership_discount_bps");
    return QStringLiteral("SELECT id, phone, nickname, avatar_path, balance_fen, %1, status, created_at FROM user")
        .arg(membership);
}

}

std::optional<UserProfile> UserRepository::findByPhone(
    QSqlDatabase &database, const QString &phone, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(userSelectSql(database) + QStringLiteral(" WHERE phone = :phone"));
    query.bindValue(QStringLiteral(":phone"), phone);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return mapUser(query);
}

std::optional<UserProfile> UserRepository::findById(
    QSqlDatabase &database, qint64 userId, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(userSelectSql(database) + QStringLiteral(" WHERE id = :userId"));
    query.bindValue(QStringLiteral(":userId"), userId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return mapUser(query);
}

bool UserRepository::createAutoRegistered(QSqlDatabase &database,
                                           const QString &phone,
                                           const QString &now,
                                           QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO user (phone, nickname, balance_fen, status, last_login_at, "
        "created_at, updated_at) "
        "VALUES (:phone, :nickname, 0, 'NORMAL', :now, :now, :now)"));
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":nickname"),
                    QStringLiteral("用户") + phone.right(4));
    query.bindValue(QStringLiteral(":now"), now);

    if (query.exec()) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool UserRepository::updateLastLogin(QSqlDatabase &database, qint64 userId,
                                     const QString &now,
                                     QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE user SET last_login_at = :now, updated_at = :now "
        "WHERE id = :userId"));
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":userId"), userId);

    if (query.exec() && query.numRowsAffected() == 1) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool UserRepository::refreshMembership(QSqlDatabase &database, UserProfile *user,
                                       const QString &now, QString *errorMessage) const
{
    if (!user) {
        if (errorMessage) *errorMessage = QStringLiteral("user is required");
        return false;
    }

    const QDateTime nowDt = QDateTime::fromString(
        now, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QDateTime expires = QDateTime::fromString(
        user->membershipExpiresAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const bool active = nowDt.isValid() && expires.isValid() && expires > nowDt;
    const int remainingDays = active
        ? qMax(1, qCeil(nowDt.secsTo(expires) / 86400.0)) : 0;
    const QString expiresAt = active
        ? expires.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    const int discountBps = active
        ? qBound(0, user->membershipDiscountBps, 10000) : 10000;

    if (user->isMember != active || user->membershipRemainingDays != remainingDays
        || user->membershipExpiresAt != expiresAt
        || user->membershipDiscountBps != discountBps) {
        QSqlQuery query(database);
        query.prepare(QStringLiteral(
            "UPDATE user SET is_member=:isMember, membership_remaining_days=:days, "
            "membership_expires_at=:expiresAt, membership_discount_bps=:discount, "
            "updated_at=:now WHERE id=:userId"));
        query.bindValue(QStringLiteral(":isMember"), active ? 1 : 0);
        query.bindValue(QStringLiteral(":days"), remainingDays);
        query.bindValue(QStringLiteral(":expiresAt"),
                        expiresAt.isEmpty() ? QVariant() : expiresAt);
        query.bindValue(QStringLiteral(":discount"), discountBps);
        query.bindValue(QStringLiteral(":now"), now);
        query.bindValue(QStringLiteral(":userId"), user->userId);
        if (!query.exec() || query.numRowsAffected() != 1) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }

    user->isMember = active;
    user->membershipRemainingDays = remainingDays;
    user->membershipExpiresAt = expiresAt;
    user->membershipDiscountBps = discountBps;
    return true;
}

bool UserRepository::updateNickname(QSqlDatabase &database, qint64 userId,
                                    const QString &nickname, const QString &now,
                                    QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE user SET nickname = :nickname, updated_at = :now "
        "WHERE id = :userId"));
    query.bindValue(QStringLiteral(":nickname"), nickname);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":userId"), userId);

    if (query.exec() && query.numRowsAffected() == 1) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool UserRepository::updateAvatarPath(QSqlDatabase &database, qint64 userId,
                                      const QString &avatarPath, const QString &now,
                                      QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE user SET avatar_path = :avatarPath, updated_at = :now "
        "WHERE id = :userId"));
    query.bindValue(QStringLiteral(":avatarPath"), avatarPath);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":userId"), userId);
    if (query.exec() && query.numRowsAffected() == 1) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool UserRepository::increaseBalance(QSqlDatabase &database, qint64 userId,
                                     qint64 amountFen, const QString &now,
                                     qint64 *balanceFen,
                                     QString *errorMessage) const
{
    if (!balanceFen) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("balance output is unavailable");
        }
        return false;
    }
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE user SET balance_fen = balance_fen + :amount, updated_at = :now "
        "WHERE id = :userId"));
    update.bindValue(QStringLiteral(":amount"), amountFen);
    update.bindValue(QStringLiteral(":now"), now);
    update.bindValue(QStringLiteral(":userId"), userId);
    if (!update.exec() || update.numRowsAffected() != 1) {
        if (errorMessage) {
            *errorMessage = update.lastError().text();
        }
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT balance_fen FROM user WHERE id = :userId"));
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *balanceFen = query.value(0).toLongLong();
    return true;
}

bool UserRepository::insertRechargeRecord(QSqlDatabase &database, qint64 userId,
                                          const RechargeInfo &recharge,
                                          qint64 *rechargeId,
                                          QString *errorMessage) const
{
    if (!rechargeId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("recharge ID output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO recharge_record "
        "(record_no, user_id, amount_fen, balance_after_fen, status, created_at) "
        "VALUES (:recordNo, :userId, :amount, :balance, 'SUCCESS', :createdAt)"));
    query.bindValue(QStringLiteral(":recordNo"), recharge.recordNo);
    query.bindValue(QStringLiteral(":userId"), userId);
    query.bindValue(QStringLiteral(":amount"), recharge.amountFen);
    query.bindValue(QStringLiteral(":balance"), recharge.balanceFen);
    query.bindValue(QStringLiteral(":createdAt"), recharge.createdAt);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *rechargeId = query.lastInsertId().toLongLong();
    return *rechargeId > 0;
}

bool UserRepository::decreaseBalance(QSqlDatabase &database, qint64 userId,
                                     qint64 amountFen, const QString &now,
                                     bool *deducted,
                                     QString *errorMessage) const
{
    if (!deducted) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("deduction output is unavailable");
        }
        return false;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "UPDATE user SET balance_fen = balance_fen - :amount, updated_at = :now "
        "WHERE id = :userId AND balance_fen >= :amount"));
    query.bindValue(QStringLiteral(":amount"), amountFen);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":userId"), userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *deducted = query.numRowsAffected() == 1;
    return true;
}

bool UserRepository::hasActiveOrder(QSqlDatabase &database, qint64 userId,
                                    bool *hasActiveOrder,
                                    QString *errorMessage) const
{
    if (!hasActiveOrder) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("active order output is unavailable");
        }
        return false;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT 1 FROM charging_order "
        "WHERE user_id = :userId "
        "AND status IN ('CREATED', 'CHARGING', 'PENDING_PAYMENT') LIMIT 1"));
    query.bindValue(QStringLiteral(":userId"), userId);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    *hasActiveOrder = query.next();
    return true;
}

QJsonArray UserRepository::listForAdmin(QSqlDatabase &database,
                                         const QString &phoneKeyword,
                                         QString *errorMessage) const
{
    QString keyword = phoneKeyword.trimmed();
    keyword.replace('\\', QStringLiteral("\\\\"));
    keyword.replace('%', QStringLiteral("\\%"));
    keyword.replace('_', QStringLiteral("\\_"));
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id, phone, nickname, balance_fen, created_at, status "
                                 "FROM user WHERE phone LIKE :keyword ESCAPE '\\' ORDER BY id"));
    query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%") + keyword + QStringLiteral("%"));
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return {};
    }
    QJsonArray users;
    while (query.next()) {
        users.append(QJsonObject{{QStringLiteral("userId"), query.value(0).toLongLong()},
                                 {QStringLiteral("phone"), query.value(1).toString()},
                                 {QStringLiteral("nickname"), query.value(2).toString()},
                                 {QStringLiteral("balanceFen"), query.value(3).toLongLong()},
                                 {QStringLiteral("createdAt"), query.value(4).toString()},
                                 {QStringLiteral("status"), query.value(5).toString()}});
    }
    return users;
}

QJsonObject UserRepository::statusForAdmin(QSqlDatabase &database, qint64 userId,
                                            QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT phone, status FROM user WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return {};
    }
    if (!query.next()) return {};
    return {{QStringLiteral("phone"), query.value(0).toString()},
            {QStringLiteral("status"), query.value(1).toString()}};
}

bool UserRepository::compareAndSetStatus(QSqlDatabase &database, qint64 userId,
                                         const QString &before, const QString &after,
                                         const QString &now, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral("UPDATE user SET status = :after, updated_at = :now "
                                 "WHERE id = :id AND status = :before"));
    query.bindValue(QStringLiteral(":after"), after);
    query.bindValue(QStringLiteral(":now"), now);
    query.bindValue(QStringLiteral(":id"), userId);
    query.bindValue(QStringLiteral(":before"), before);
    if (!query.exec()) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() == 1;
}

UserProfile UserRepository::mapUser(const QSqlQuery &query)
{
    UserProfile user;
    user.userId = query.value(0).toLongLong();
    user.phone = query.value(1).toString();
    user.nickname = query.value(2).toString();
    user.avatarPath = query.value(3).toString();
    user.balanceFen = query.value(4).toLongLong();
    user.isMember = query.value(5).toInt() != 0;
    user.membershipRemainingDays = query.value(6).toInt();
    user.membershipExpiresAt = query.value(7).toString();
    user.membershipDiscountBps = query.value(8).toInt();
    user.status = query.value(9).toString();
    user.createdAt = query.value(10).toString();
    return user;
}

QJsonArray UserRepository::membershipProducts(QSqlDatabase &database, QString *errorMessage) const
{
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT id, product_no, name, card_type, duration_days, sale_price_fen, service_fee_discount_bps FROM membership_product WHERE status='ON_SALE' ORDER BY duration_days"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return {};
    }
    QJsonArray result;
    while (query.next()) result.append(QJsonObject{{QStringLiteral("productId"), query.value(0).toLongLong()}, {QStringLiteral("productNo"), query.value(1).toString()}, {QStringLiteral("name"), query.value(2).toString()}, {QStringLiteral("cardType"), query.value(3).toString()}, {QStringLiteral("durationDays"), query.value(4).toInt()}, {QStringLiteral("salePriceFen"), query.value(5).toLongLong()}, {QStringLiteral("serviceFeeDiscountBps"), query.value(6).toInt()}});
    return result;
}
