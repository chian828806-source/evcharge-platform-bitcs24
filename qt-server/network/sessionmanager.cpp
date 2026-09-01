/*
 * 功能：实现线程安全的Session创建、查询、删除和清空。
 * 安全：令牌来自QUuid随机值，调用方只拿到字符串，不接触内部表。
 */
#include "sessionmanager.h"

#include <QReadLocker>
#include <QUuid>
#include <QWriteLocker>

QString SessionManager::createSession(qint64 principalId, SessionRole role)
{
    // WithoutBraces便于JSON传输；S-前缀方便日志识别。
    const QString sessionId =
        QStringLiteral("S-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    // 写锁保护QHash修改，锁作用域由locker自动管理。
    QWriteLocker locker(&m_lock);
    m_sessions.insert(sessionId, {principalId, role});
    return sessionId;
}

bool SessionManager::findSession(const QString &sessionId,
                                 SessionContext *context) const
{
    // 多个请求可以并发读Session，因此查询只获取共享读锁。
    QReadLocker locker(&m_lock);
    const auto iterator = m_sessions.constFind(sessionId);
    if (iterator == m_sessions.cend()) {
        return false;
    }
    // 输出指针可为空，便于调用方只检查Session是否存在。
    if (context) {
        *context = iterator.value();
    }
    return true;
}

void SessionManager::removeSession(const QString &sessionId)
{
    // 删除会改变QHash，必须持有独占写锁。
    QWriteLocker locker(&m_lock);
    m_sessions.remove(sessionId);
}

void SessionManager::clear()
{
    // 服务端退出或测试清理时一次性失效所有Session。
    QWriteLocker locker(&m_lock);
    m_sessions.clear();
}
