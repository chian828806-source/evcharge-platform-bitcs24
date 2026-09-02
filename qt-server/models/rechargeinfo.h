/*
 * 功能：定义用户充值成功后返回的流水摘要。
 * 边界：模型只保存数据和Socket字段转换，不执行余额更新或SQL。
 */
#pragma once

#include <QJsonObject>
#include <QString>

struct RechargeInfo
{
    qint64 rechargeId = 0;
    QString recordNo;
    qint64 amountFen = 0;
    qint64 balanceFen = 0;
    QString createdAt;

    QJsonObject toJson() const
    {
        return {
            {QStringLiteral("rechargeId"), rechargeId},
            {QStringLiteral("recordNo"), recordNo},
            {QStringLiteral("amountFen"), amountFen},
            {QStringLiteral("balanceFen"), balanceFen},
            {QStringLiteral("createdAt"), createdAt}
        };
    }
};
