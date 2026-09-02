/*
 * 功能：实现/dashboard连接检查、主题订阅和运营数据推送。
 * 协议：消息结构完全来自docs/03-API.md，不直接读取SQLite。
 */
#include "dashboardwebsocketserver.h"

#include "shared/protocol/errorcodes.h"
#include "shared/protocol/messagetypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QWebSocket>
#include <QWebSocketServer>

DashboardWebSocketServer::DashboardWebSocketServer(QObject *parent)
    : QObject(parent),
      m_server(new QWebSocketServer(
          QStringLiteral("EVCharge Dashboard"),
          QWebSocketServer::NonSecureMode, this))
{
    // QWebSocketServer运行在非TLS演示模式，适合项目内网环境。
    connect(m_server, &QWebSocketServer::newConnection,
            this, &DashboardWebSocketServer::acceptConnection);
}

DashboardWebSocketServer::~DashboardWebSocketServer()
{
    // 主程序退出前关闭浏览器连接和监听端口。
    close();
}

bool DashboardWebSocketServer::listen(quint16 port)
{
    // Any允许宿主机浏览器通过虚拟机IP访问大屏服务。
    return m_server->listen(QHostAddress::Any, port);
}

void DashboardWebSocketServer::close()
{
    // 复制key列表，避免关闭信号触发时遍历中的容器被修改。
    const auto sockets = m_subscriptions.keys();
    for (QWebSocket *socket : sockets) {
        socket->close();
    }
    m_subscriptions.clear();
    m_server->close();
}

QString DashboardWebSocketServer::errorString() const
{
    return m_server->errorString();
}

void DashboardWebSocketServer::publish(const QString &topic,
                                       const QJsonObject &data)
{
    // 未登记主题不能对外发送，防止前后端出现私有契约。
    if (!MessageTypes::dashboardTopics().contains(topic)) {
        return;
    }
    // 每次广播都套用统一DASHBOARD_UPDATE封装。
    const QJsonObject update{
        {QStringLiteral("type"), MessageTypes::DashboardUpdate},
        {QStringLiteral("topic"), topic},
        {QStringLiteral("data"), data}
    };
    // 只向显式订阅该topic的连接发送，减少无关数据。
    for (auto iterator = m_subscriptions.cbegin();
         iterator != m_subscriptions.cend(); ++iterator) {
        if (iterator.value().contains(topic)) {
            sendJson(iterator.key(), update);
        }
    }
}

void DashboardWebSocketServer::acceptConnection()
{
    // Qt已完成WebSocket升级，这里取得等待处理的连接。
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }
    // 文档只允许/dashboard路径，其他路径立即按策略错误关闭。
    if (socket->requestUrl().path() != QStringLiteral("/dashboard")) {
        socket->close(QWebSocketProtocol::CloseCodePolicyViolated,
                      QStringLiteral("invalid WebSocket path"));
        socket->deleteLater();
        return;
    }

    // 初始为空集合，客户端必须先发送订阅消息才会收到推送。
    m_subscriptions.insert(socket, {});
    connect(socket, &QWebSocket::textMessageReceived,
            this, &DashboardWebSocketServer::handleTextMessage);
    connect(socket, &QWebSocket::disconnected,
            this, &DashboardWebSocketServer::removeConnection);
}

void DashboardWebSocketServer::handleTextMessage(const QString &message)
{
    // sender就是发来当前文本帧的浏览器Socket。
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket) {
        return;
    }

    // WebSocket本身已有消息边界，只需要解析单个文本帧JSON。
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(message.toUtf8(), &parseError);
    const QJsonObject request = document.object();
    const QString requestId =
        request.value(QStringLiteral("requestId")).toString();

    // 局部函数统一订阅阶段的错误响应格式。
    auto sendError = [this, socket, &requestId](const QString &text) {
        sendJson(socket, {
            {QStringLiteral("requestId"), requestId},
            {QStringLiteral("code"), ErrorCodes::InvalidSocketMessage},
            {QStringLiteral("message"), text},
            {QStringLiteral("data"), QJsonObject()}
        });
    };

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        sendError(QStringLiteral("invalid JSON message"));
        return;
    }
    if (!request.value(QStringLiteral("requestId")).isString()
        || requestId.isEmpty()
        || !request.value(QStringLiteral("payload")).isObject()) {
        sendError(QStringLiteral("invalid dashboard request envelope"));
        return;
    }
    if (request.value(QStringLiteral("type")).toString()
        != MessageTypes::DashboardSubscribe) {
        sendError(QStringLiteral("unsupported WebSocket message type"));
        return;
    }

    // payload.topics必须是由文档登记主题组成的非空数组。
    const QJsonArray topicsValue =
        request.value(QStringLiteral("payload")).toObject()
            .value(QStringLiteral("topics")).toArray();
    QSet<QString> topics;
    for (const QJsonValue &value : topicsValue) {
        if (!value.isString()
            || !MessageTypes::dashboardTopics().contains(value.toString())) {
            sendError(QStringLiteral("invalid dashboard topic"));
            return;
        }
        topics.insert(value.toString());
    }
    if (topics.isEmpty()) {
        sendError(QStringLiteral("topics must not be empty"));
        return;
    }

    // 只有全部主题通过验证后才整体替换订阅，避免部分成功。
    m_subscriptions[socket] = topics;
    sendJson(socket, {
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("code"), ErrorCodes::Success},
        {QStringLiteral("message"), QStringLiteral("success")},
        {QStringLiteral("data"), QJsonObject{
            {QStringLiteral("topics"), QJsonArray::fromStringList(topics.values())}
        }}
    });
}

void DashboardWebSocketServer::removeConnection()
{
    // 从订阅表移除，后续publish就不会再访问失效Socket。
    QWebSocket *socket = qobject_cast<QWebSocket *>(sender());
    if (!socket) {
        return;
    }
    m_subscriptions.remove(socket);
    socket->deleteLater();
}

void DashboardWebSocketServer::sendJson(QWebSocket *socket,
                                        const QJsonObject &json)
{
    // WebSocket使用文本帧，不需要TCP JSON Lines末尾的换行符。
    socket->sendTextMessage(
        QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact)));
}
