/*
 * 功能：实现用户资料查询、自动注册和资料更新SQL。
 */
#include "userrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

std::optional<UserProfile> UserRepository::findByPhone(
    QSqlDatabase &database, const QString &phone, QString *errorMessage) const
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_fen, status, created_at "
        "FROM user WHERE phone = :phone"));
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
    query.prepare(QStringLiteral(
        "SELECT id, phone, nickname, avatar_path, balance_fen, status, created_at "
        "FROM user WHERE id = :userId"));
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
    user.status = query.value(5).toString();
    user.createdAt = query.value(6).toString();
    return user;
}
