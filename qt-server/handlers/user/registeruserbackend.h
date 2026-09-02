/* Owns the User-side handlers and their services for the lifetime of the server. */
#pragma once

#include <QString>

class DatabaseManager;
class MessageDispatcher;
class SessionManager;

class UserBackendRegistry
{
public:
    UserBackendRegistry(DatabaseManager *databaseManager, SessionManager *sessions,
                        MessageDispatcher *dispatcher,
                        const QString &avatarDirectory = QStringLiteral("data/avatars"));

private:
    class Impl;
    Impl *m_impl;
};
