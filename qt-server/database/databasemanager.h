/*
 * 功能：为每个线程提供独立命名的SQLite连接，并开启外键约束。
 * 边界：只管理连接，不包含任何用户、订单或管理员业务规则。
 */
#pragma once

#include <QMutex>
#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath,
                             QString connectionPrefix = QStringLiteral("evcharge-server"));

    // 获取当前线程专用连接；失败时返回false并写入errorMessage。
    bool database(QSqlDatabase *database, QString *errorMessage) const;
    QString databasePath() const;

private:
    QString connectionNameForCurrentThread() const;

    QString m_databasePath;
    QString m_connectionPrefix;
    mutable QMutex m_connectionMutex;
};
