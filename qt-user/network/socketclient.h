/*
 * 功能：为Qt用户端页面提供统一TCP连接、请求发送和响应接收接口。
 * 边界：页面不直接操作QTcpSocket，业务含义由页面或Model处理。
 */
#pragma once

#include "shared/protocol/jsonlinecodec.h"

#include <QJsonObject>
#include <QDateTime>
#include <QHash>
#include <QObject>

class QTcpSocket;
class QTimer;

// SocketClient把底层异步Socket转换为页面易用的Qt信号接口。
class SocketClient : public QObject
{
    Q_OBJECT

public:
    // 创建内部QTcpSocket并连接底层事件信号。
    explicit SocketClient(QObject *parent = nullptr);

    // 发起异步连接；结果通过connected或socketError信号返回。
    void connectToServer(const QString &host, quint16 port);
    // 请求Qt优雅断开当前连接。
    void disconnectFromServer();
    // 供页面决定当前是否允许发送请求。
    bool isConnected() const;

    // 发送标准请求并返回requestId；未连接时返回空字符串。
    QString sendRequest(const QString &type, const QString &sessionId,
                        const QJsonObject &payload,
                        const QString &requestId = {}, int timeoutMs = 5000);

signals:
    // 连接状态与解析结果都通过信号通知UI，避免阻塞界面线程。
    void connected();
    void disconnected();
    void responseReceived(const QJsonObject &response);
    void protocolError(const QString &message);
    void socketError(const QString &message);
    // 请求超过约定时间仍未收到响应时，页面可停止等待并提示用户。
    void requestTimedOut(const QString &requestId, const QString &type);
    // 断线等情况导致未完成请求失败时，通知对应页面清理等待状态。
    void requestFailed(const QString &requestId, const QString &type,
                       const QString &message);

private slots:
    // 读取服务端字节并拆成一条或多条完整响应。
    void readAvailableData();

private:
    QTcpSocket *m_socket = nullptr;
    JsonLineCodec m_codec;

    struct PendingRequest {
        QString type;
        QDateTime sentAt;
        QTimer *timer = nullptr;
    };
    QHash<QString, PendingRequest> m_pendingRequests;

    void clearPendingRequest(const QString &requestId);
    void failAllPending(const QString &message);
};
