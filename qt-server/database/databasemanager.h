#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    explicit DatabaseManager(QString connectionName = QStringLiteral("server-main"));
    ~DatabaseManager();

    bool open(const QString &databasePath, QString *errorMessage = nullptr);
    QSqlDatabase database() const;

private:
    QString m_connectionName;
};
