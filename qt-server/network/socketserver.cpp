/*
 * 功能：实现TCP监听和连接接收。
 * 设计：一个SocketServer共享Dispatcher，每个连接拥有独立ClientSession。
 */
#include "socketserver.h"

#include "clientsession.h"

#include <QTcpServer>
#include <QTcpSocket>

SocketServer::SocketServer(MessageDispatcher *dispatcher, QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this)), m_dispatcher(dispatcher)
{
    // newConnection只负责通知，真正的pending socket在槽函数中循环取出。
    connect(m_server, &QTcpServer::newConnection,
            this, &SocketServer::acceptConnections);
}

bool SocketServer::listen(const QHostAddress &address, quint16 port)
{
    // 监听失败原因由QTcpServer保留，调用方通过errorString显示。
    return m_server->listen(address, port);
}

void SocketServer::close()
{
    // close不会粗暴销毁已建立连接，便于后续实现优雅停机。
    m_server->close();
}

QString SocketServer::errorString() const
{
    return m_server->errorString();
}

void SocketServer::acceptConnections()
{
    // 一个事件中可能积累多个连接，因此必须循环处理。
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        // ClientSession接管socket所有权并绑定统一Dispatcher。
        new ClientSession(socket, m_dispatcher, this);
    }
}
