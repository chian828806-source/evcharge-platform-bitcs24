/*
 * 功能：集中处理user和用户活动订单检查所需的参数化SQL。
 * 边界：不生成Session，不决定冻结用户是否可登录，不拼装网络响应。
 */
#pragma once

#include "models/rechargeinfo.h"
#include "models/userprofile.h"

#include <QSqlDatabase>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <optional>

class QSqlQuery;

class UserRepository
{
public:
    std::optional<UserProfile> findByPhone(QSqlDatabase &database,
                                           const QString &phone,
                                           QString *errorMessage) const;
    std::optional<UserProfile> findById(QSqlDatabase &database,
                                        qint64 userId,
                                        QString *errorMessage) const;

    bool createAutoRegistered(QSqlDatabase &database, const QString &phone,
                              const QString &now, QString *errorMessage) const;
    bool updateLastLogin(QSqlDatabase &database, qint64 userId,
                         const QString &now, QString *errorMessage) const;
    bool updateNickname(QSqlDatabase &database, qint64 userId,
                        const QString &nickname, const QString &now,
                        QString *errorMessage) const;
    bool updateAvatarPath(QSqlDatabase &database, qint64 userId,
                          const QString &avatarPath, const QString &now,
                          QString *errorMessage) const;
    bool increaseBalance(QSqlDatabase &database, qint64 userId, qint64 amountFen,
                         const QString &now, qint64 *balanceFen,
                         QString *errorMessage) const;
    bool insertRechargeRecord(QSqlDatabase &database, qint64 userId,
                              const RechargeInfo &recharge, qint64 *rechargeId,
                              QString *errorMessage) const;
    bool decreaseBalance(QSqlDatabase &database, qint64 userId, qint64 amountFen,
                         const QString &now, bool *deducted,
                         QString *errorMessage) const;
    bool hasActiveOrder(QSqlDatabase &database, qint64 userId,
                        bool *hasActiveOrder, QString *errorMessage) const;

    // Shared Admin management queries; this remains the sole repository for user data.
    QJsonArray listForAdmin(QSqlDatabase &database, const QString &phoneKeyword,
                            QString *errorMessage) const;
    QJsonObject statusForAdmin(QSqlDatabase &database, qint64 userId,
                               QString *errorMessage) const;
    bool compareAndSetStatus(QSqlDatabase &database, qint64 userId,
                             const QString &before, const QString &after,
                             const QString &now, QString *errorMessage) const;

private:
    static UserProfile mapUser(const QSqlQuery &query);
};
