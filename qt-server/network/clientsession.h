/*
 * 功能：管理一个TCP客户端连接的读取缓冲、请求解析、分发和响应写回。
 * 生命周期：Socket断开后ClientSession自动deleteLater，不残留半包。
 */
#pragma once

#include "shared/protocol/jsonlinecodec.h"

#include <QObject>

class MessageDispatcher;
class QTcpSocket;

// 每个已连接QTcpSocket对应一个ClientSession对象。
class ClientSession : public QObject
{
    Q_OBJECT

public:
    // 接管一个已连接Socket，并使用共享Dispatcher处理其请求。
    ClientSession(QTcpSocket *socket, MessageDispatcher *dispatcher,
                  QObject *parent = nullptr);

private slots:
    // 读取本次到达的全部字节并交给JsonLineCodec。
    void readAvailableData();
    // 对端断开时安排当前会话对象安全销毁。
    void closeSession();

private:
    // 解析单个完整JSON帧并调用Dispatcher。
    void processFrame(const QByteArray &frame);
    // 统一生成4401协议错误，避免不同入口返回不同格式。
    void sendProtocolError(const QString &requestId, const QString &message);

    QTcpSocket *m_socket = nullptr;
    MessageDispatcher *m_dispatcher = nullptr;
    JsonLineCodec m_codec;
};
