/*
 * 功能：保存登录后生成的Session，并区分普通用户和管理员角色。
 * 线程：内部使用读写锁，可由网络线程和业务线程并发查询。
 */
#pragma once

#include <QHash>
#include <QReadWriteLock>
#include <QString>

// Session中只保存鉴权需要的最小身份信息，不保存业务对象。
enum class SessionRole {
    User,
    Admin
};

// Dispatcher验证成功后传给Handler的可信身份。
struct SessionContext {
    qint64 principalId = 0;
    SessionRole role = SessionRole::User;
};

// SessionManager负责令牌生命周期，不负责校验用户名或密码。
class SessionManager
{
public:
    // 为指定主体创建不可预测的随机Session编号。
    QString createSession(qint64 principalId, SessionRole role);
    // 查询Session并复制身份上下文；不存在时返回false。
    bool findSession(const QString &sessionId, SessionContext *context) const;
    // 注销单个Session。
    void removeSession(const QString &sessionId);
    // 服务关闭或测试结束时清空全部Session。
    void clear();

private:
    mutable QReadWriteLock m_lock;
    QHash<QString, SessionContext> m_sessions;
};
