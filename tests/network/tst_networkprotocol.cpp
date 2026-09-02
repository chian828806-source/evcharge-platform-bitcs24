/*
 * 功能：验证JSON Lines、消息封装、Session角色和Dispatcher边界。
 * 运行：每个检查输出PASS/FAIL，任意失败都会返回非零退出码。
 */
#include "qt-server/network/messagedispatcher.h"
#include "qt-server/network/sessionmanager.h"
#include "qt-user/network/socketclient.h"
#include "qt-admin/network/adminsocketclient.h"
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/jsonlinecodec.h"
#include "shared/protocol/messagetypes.h"
#include "shared/protocol/protocolmessage.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QEventLoop>
#include <QTcpServer>
#include <QTimer>
#include <QTextStream>

namespace {

// 使用简单计数器避免引入额外测试框架，虚拟机只需QtCore即可运行。
int failures = 0;

void check(bool condition, const QString &name)
{
    // 成功写标准输出，失败写标准错误，方便人工和脚本同时阅读。
    QTextStream stream(condition ? stdout : stderr);
    stream << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition) {
        ++failures;
    }
}

void testJsonLinesHandlesSplitAndStickyPackets()
{
    // 第一次只喂入前半条消息，验证Codec不会提前输出。
    JsonLineCodec codec;
    bool overflow = false;
    const QByteArray first = R"({"requestId":"1","type":"USER_LOGIN",)";
    check(codec.append(first, &overflow).isEmpty() && !overflow,
          QStringLiteral("split frame waits for newline"));

    // 第二次补齐第一条并紧跟第二条，模拟半包与粘包同时出现。
    const QByteArray rest =
        R"("sessionId":null,"payload":{"phone":"13800138000"}})"
        "\n"
        R"({"requestId":"2","type":"USER_LOGIN","sessionId":null,"payload":{"phone":"13800138001"}})"
        "\n";
    const QList<QByteArray> frames = codec.append(rest, &overflow);
    check(frames.size() == 2 && !overflow,
          QStringLiteral("split and sticky packets produce two frames"));
    // 分帧成功后，每一帧还必须是独立可解析的JSON对象。
    check(QJsonDocument::fromJson(frames.at(0)).isObject()
              && QJsonDocument::fromJson(frames.at(1)).isObject(),
          QStringLiteral("decoded frames contain JSON objects"));
}

void testJsonLinesRejectsOversizedTrailingPartialFrame()
{
    JsonLineCodec codec;
    bool overflow = false;
    const QByteArray bytes = QByteArray("{}\n")
        + QByteArray(JsonLineCodec::MaxBufferedBytes + 1, 'x');
    check(codec.append(bytes, &overflow).isEmpty() && overflow,
          QStringLiteral("oversized trailing partial frame is rejected"));
}

void testRequestValidationAndResponseShape()
{
    // 构造与docs/03-API.md登录示例一致的公共请求外壳。
    const QJsonObject json{
        {QStringLiteral("requestId"), QStringLiteral("REQ-001")},
        {QStringLiteral("type"), MessageTypes::UserLogin},
        {QStringLiteral("sessionId"), QJsonValue(QJsonValue::Null)},
        {QStringLiteral("payload"), QJsonObject{
            {QStringLiteral("phone"), QStringLiteral("13800138000")}
        }}
    };
    // 验证JSON到C++对象的转换及null sessionId处理。
    RequestMessage request;
    QString error;
    check(RequestMessage::fromJson(json, &request, &error),
          QStringLiteral("valid request envelope is accepted"));
    check(request.requestId == QStringLiteral("REQ-001")
              && request.sessionId.isEmpty(),
          QStringLiteral("request fields are preserved"));

    QJsonObject missingSession = json;
    missingSession.remove(QStringLiteral("sessionId"));
    check(!RequestMessage::fromJson(missingSession, &request, &error),
          QStringLiteral("missing sessionId is rejected"));

    // 验证成功响应始终包含code=200和对象类型data。
    const QJsonObject response =
        ResponseMessage::success(request.requestId).toJson();
    check(response.value(QStringLiteral("code")).toInt()
              == ErrorCodes::Success
              && response.value(QStringLiteral("data")).isObject(),
          QStringLiteral("response follows public contract"));
}

void testSessionAndDispatcherBoundaries()
{
    // 注册一个最小用户路由，用来隔离测试网络鉴权而非业务逻辑。
    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    dispatcher.registerHandler(
        MessageTypes::UserProfileGet, MessageDispatcher::Access::User,
        [](const RequestMessage &request, const SessionContext &context) {
            return ResponseMessage::success(request.requestId, {
                {QStringLiteral("userId"), context.principalId}
            });
        });

    // 同一请求依次使用空Session、用户Session和管理员Session。
    RequestMessage request{
        QStringLiteral("REQ-002"),
        MessageTypes::UserProfileGet,
        {},
        {}
    };
    check(dispatcher.dispatch(request).code == ErrorCodes::InvalidSession,
          QStringLiteral("protected route rejects missing session"));

    // 普通用户身份应被传给Handler并进入响应数据。
    request.sessionId = sessions.createSession(7, SessionRole::User);
    const ResponseMessage userResponse = dispatcher.dispatch(request);
    check(userResponse.code == ErrorCodes::Success
              && userResponse.data.value(QStringLiteral("userId")).toInt() == 7,
          QStringLiteral("user route receives authenticated identity"));

    // 管理员Session不能调用声明为User的路由。
    request.sessionId = sessions.createSession(1, SessionRole::Admin);
    check(dispatcher.dispatch(request).code == ErrorCodes::InvalidSession,
          QStringLiteral("role mismatch is rejected"));
}

void testKnownMessageRegistry()
{
    // 数量变化意味着公共文档与代码可能发生漏登或私自扩展。
    check(MessageTypes::tcpTypes().size() == 29,
          QStringLiteral("all 29 documented TCP message types are registered"));
    check(MessageTypes::dashboardTopics().size() == 4,
          QStringLiteral("all four dashboard topics are registered"));
}

void testUserClientRequestTimeout()
{
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost, 0),
          QStringLiteral("local timeout test server listens"));

    SocketClient client;
    QEventLoop connectedLoop;
    QObject::connect(&client, &SocketClient::connected,
                     &connectedLoop, &QEventLoop::quit);
    client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    QTimer::singleShot(1000, &connectedLoop, &QEventLoop::quit);
    connectedLoop.exec();
    check(client.isConnected(), QStringLiteral("user client connects asynchronously"));

    QString timedOutId;
    QString timedOutType;
    QEventLoop timeoutLoop;
    QObject::connect(&client, &SocketClient::requestTimedOut,
                     &timeoutLoop,
                     [&](const QString &requestId, const QString &type) {
        timedOutId = requestId;
        timedOutType = type;
        timeoutLoop.quit();
    });
    const QString requestId = client.sendRequest(
        MessageTypes::UserProfileGet, QStringLiteral("S-test"), {}, {}, 30);
    QTimer::singleShot(1000, &timeoutLoop, &QEventLoop::quit);
    timeoutLoop.exec();
    check(!requestId.isEmpty() && timedOutId == requestId
              && timedOutType == MessageTypes::UserProfileGet,
          QStringLiteral("user client reports request timeout"));
}

void testAdminClientRequestTimeout()
{
    QTcpServer server;
    check(server.listen(QHostAddress::LocalHost, 0),
          QStringLiteral("local admin timeout test server listens"));

    AdminSocketClient client;
    QEventLoop connectedLoop;
    QObject::connect(&client, &AdminSocketClient::connected,
                     &connectedLoop, &QEventLoop::quit);
    client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    QTimer::singleShot(1000, &connectedLoop, &QEventLoop::quit);
    connectedLoop.exec();
    check(client.isConnected(), QStringLiteral("admin client connects asynchronously"));

    QString timedOutId;
    QEventLoop timeoutLoop;
    QObject::connect(&client, &AdminSocketClient::requestTimedOut,
                     &timeoutLoop,
                     [&](const QString &requestId, const QString &) {
        timedOutId = requestId;
        timeoutLoop.quit();
    });
    const QString requestId = client.sendRequest(
        MessageTypes::AdminRevenueSummary, QStringLiteral("S-admin"), {}, {}, 30);
    QTimer::singleShot(1000, &timeoutLoop, &QEventLoop::quit);
    timeoutLoop.exec();
    check(!requestId.isEmpty() && timedOutId == requestId,
          QStringLiteral("admin client reports request timeout"));
}

}

int main(int argc, char *argv[])
{
    // QCoreApplication初始化Qt基础设施，但测试不进入长期事件循环。
    QCoreApplication application(argc, argv);
    // 每组测试关注一个独立的小功能，失败时继续运行以给出完整报告。
    testJsonLinesHandlesSplitAndStickyPackets();
    testJsonLinesRejectsOversizedTrailingPartialFrame();
    testRequestValidationAndResponseShape();
    testSessionAndDispatcherBoundaries();
    testKnownMessageRegistry();
    testUserClientRequestTimeout();
    testAdminClientRequestTimeout();
    // 非零退出码可直接用于CI或提交前检查。
    QTextStream(stdout) << "TOTAL_FAILURES=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
