/*
 * 功能：实现SQLite连接创建、打开和PRAGMA foreign_keys设置。
 */
#include "databasemanager.h"

#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

#include <utility>

DatabaseManager::DatabaseManager(QString databasePath, QString connectionPrefix)
    : m_databasePath(std::move(databasePath)),
      m_connectionPrefix(std::move(connectionPrefix))
{
}

bool DatabaseManager::database(QSqlDatabase *database,
                               QString *errorMessage) const
{
    if (!database) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("database output is unavailable");
        }
        return false;
    }

    const QString connectionName = connectionNameForCurrentThread();
    QMutexLocker locker(&m_connectionMutex);

    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(m_databasePath);
    }

    if (!db.isOpen() && !db.open()) {
        if (errorMessage) {
            *errorMessage = db.lastError().text();
        }
        return false;
    }

    QSqlQuery foreignKeyQuery(db);
    if (!foreignKeyQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (errorMessage) {
            *errorMessage = foreignKeyQuery.lastError().text();
        }
        return false;
    }

    *database = db;
    return true;
}

QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

QString DatabaseManager::connectionNameForCurrentThread() const
{
    return m_connectionPrefix + QLatin1Char('-')
        + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);
}
