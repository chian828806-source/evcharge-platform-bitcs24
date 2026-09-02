/*
 * 功能：验证JSON Lines、消息封装、Session角色和Dispatcher边界。
 * 运行：每个检查输出PASS/FAIL，任意失败都会返回非零退出码。
 */
#include "qt-server/network/messagedispatcher.h"
#include "qt-server/network/sessionmanager.h"
#include "qt-server/database/databasemanager.h"
#include "qt-server/handlers/user/registeruserhandlers.h"
#include "qt-server/handlers/user/registerorderhandlers.h"
#include "qt-server/handlers/user/orderhandler.h"
#include "qt-server/handlers/user/registerstationhandlers.h"
#include "qt-server/handlers/user/stationhandler.h"
#include "qt-server/handlers/user/userhandler.h"
#include "qt-server/repositories/stationrepository.h"
#include "qt-server/repositories/orderrepository.h"
#include "qt-server/repositories/predictionrepository.h"
#include "qt-server/repositories/userrepository.h"
#include "qt-server/services/user/stationservice.h"
#include "qt-server/services/user/orderservice.h"
#include "qt-server/services/user/userservice.h"
#include "qt-server/network/socketserver.h"
#include "qt-user/network/socketclient.h"
#include "qt-admin/network/adminsocketclient.h"
#include "shared/protocol/errorcodes.h"
#include "shared/protocol/jsonlinecodec.h"
#include "shared/protocol/messagetypes.h"
#include "shared/protocol/protocolmessage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QEventLoop>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTimer>
#include <QTemporaryDir>
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

void testUserLoginProfileAndNicknameFlow()
{
    // 使用当前线程专用的内存库，验证真实的Handler -> Service -> Repository链路。
    DatabaseManager databaseManager{
        QString::fromLatin1(":memory:"),
        QString::fromLatin1("user-backend-test")
    };
    QSqlDatabase database;
    QString databaseError;
    check(databaseManager.database(&database, &databaseError),
          QStringLiteral("user backend test database opens"));
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery schema(database);
    const bool userTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE user ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "phone TEXT NOT NULL UNIQUE, nickname TEXT NOT NULL, "
        "avatar_path TEXT, balance_fen INTEGER NOT NULL DEFAULT 0, "
        "status TEXT NOT NULL DEFAULT 'NORMAL', last_login_at TEXT, "
        "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"));
    const bool orderTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_order ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, "
        "status TEXT NOT NULL)"));
    check(userTableCreated && orderTableCreated,
          QStringLiteral("user backend test schema is created"));
    if (!userTableCreated || !orderTableCreated) {
        return;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserRepository userRepository;
    UserService userService(&databaseManager, &userRepository);
    UserHandler userHandler(&userService, &sessions);
    registerUserHandlers(&dispatcher, &userHandler);

    RequestMessage loginRequest{
        QStringLiteral("REQ-USER-LOGIN"),
        MessageTypes::UserLogin,
        {},
        {{QStringLiteral("phone"), QStringLiteral("13800138000")}}
    };
    const ResponseMessage loginResponse = dispatcher.dispatch(loginRequest);
    const QJsonObject loginUser = loginResponse.data.value(QStringLiteral("user")).toObject();
    const QString sessionId = loginResponse.data.value(QStringLiteral("sessionId")).toString();
    check(loginResponse.code == ErrorCodes::Success
              && loginUser.value(QStringLiteral("nickname")).toString()
                    == QStringLiteral("用户8000")
              && !sessionId.isEmpty(),
          QStringLiteral("new phone auto-registers and receives user session"));

    RequestMessage profileRequest{
        QStringLiteral("REQ-USER-PROFILE"),
        MessageTypes::UserProfileGet,
        sessionId,
        {}
    };
    const ResponseMessage profileResponse = dispatcher.dispatch(profileRequest);
    check(profileResponse.code == ErrorCodes::Success
              && profileResponse.data.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("phone")).toString()
                    == QStringLiteral("13800138000"),
          QStringLiteral("authenticated user reads own profile"));

    RequestMessage updateRequest{
        QStringLiteral("REQ-USER-NICKNAME"),
        MessageTypes::UserProfileUpdate,
        sessionId,
        {{QStringLiteral("nickname"), QStringLiteral("充电小明")}}
    };
    const ResponseMessage updateResponse = dispatcher.dispatch(updateRequest);
    check(updateResponse.code == ErrorCodes::Success
              && updateResponse.data.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("nickname")).toString()
                    == QStringLiteral("充电小明"),
          QStringLiteral("authenticated user updates nickname"));

    loginRequest.requestId = QStringLiteral("REQ-BAD-PHONE");
    loginRequest.payload.insert(QStringLiteral("phone"), QStringLiteral("123"));
    check(dispatcher.dispatch(loginRequest).code == ErrorCodes::InvalidPhone,
          QStringLiteral("invalid phone is rejected by user service"));
}

void testUserProfileWalletOrdersAndRecommendations()
{
    DatabaseManager databaseManager{
        QString::fromLatin1(":memory:"),
        QString::fromLatin1("user-extended-backend-test")
    };
    QSqlDatabase database;
    QString databaseError;
    check(databaseManager.database(&database, &databaseError),
          QStringLiteral("extended user backend test database opens"));
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery schema(database);
    const bool tablesCreated = schema.exec(QStringLiteral(
        "CREATE TABLE user (id INTEGER PRIMARY KEY, phone TEXT, nickname TEXT, "
        "avatar_path TEXT, balance_fen INTEGER, status TEXT, last_login_at TEXT, "
        "created_at TEXT, updated_at TEXT)"))
        && schema.exec(QStringLiteral(
            "CREATE TABLE recharge_record (id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "record_no TEXT UNIQUE, user_id INTEGER, amount_fen INTEGER, "
            "balance_after_fen INTEGER, status TEXT, remark TEXT, created_at TEXT)"))
        && schema.exec(QStringLiteral(
            "CREATE TABLE charging_station (id INTEGER PRIMARY KEY, station_no TEXT, "
            "name TEXT, address TEXT, district TEXT, longitude REAL, latitude REAL, "
            "price_fen_per_kwh INTEGER, service_fee_fen_per_kwh INTEGER, status TEXT)"))
        && schema.exec(QStringLiteral(
            "CREATE TABLE charging_pile (id INTEGER PRIMARY KEY, station_id INTEGER, "
            "pile_no TEXT, type TEXT, power_kw REAL, status TEXT)"))
        && schema.exec(QStringLiteral(
            "CREATE TABLE charging_order (id INTEGER PRIMARY KEY, order_no TEXT, "
            "user_id INTEGER, station_id INTEGER, pile_id INTEGER, status TEXT, "
            "price_fen_per_kwh INTEGER, service_fee_fen_per_kwh INTEGER, start_at TEXT, "
            "end_at TEXT, charge_minutes INTEGER, energy_kwh REAL, amount_fen INTEGER, "
            "created_at TEXT, updated_at TEXT)"))
        && schema.exec(QStringLiteral(
            "CREATE TABLE prediction (id INTEGER PRIMARY KEY, station_id INTEGER, "
            "prediction_time TEXT, horizon TEXT, predicted_load REAL, "
            "predicted_available_count INTEGER, peak_level TEXT, model_name TEXT, "
            "mae REAL, rmse REAL, generated_at TEXT, created_at TEXT)"));
    check(tablesCreated, QStringLiteral("extended user backend test schema is created"));
    if (!tablesCreated) {
        return;
    }

    const bool demoDataCreated = schema.exec(QStringLiteral(
        "INSERT INTO user VALUES (1, '13800138001', '用户一', NULL, 1000, 'NORMAL', "
        "NULL, '2026-01-01 00:00:00', '2026-01-01 00:00:00')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_station VALUES "
            "(1, 'S001', '近站', 'A路', 'A区', 116.397, 39.908, 100, 20, 'NORMAL'), "
            "(2, 'S002', '推荐站', 'B路', 'A区', 116.407, 39.908, 100, 20, 'NORMAL')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_pile VALUES "
            "(1, 1, 'A-01', 'FAST', 60, 'AVAILABLE'), "
            "(2, 2, 'B-01', 'FAST', 60, 'AVAILABLE')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_order VALUES "
            "(1, 'O-OLD', 1, 1, 1, 'COMPLETED', 100, 20, NULL, NULL, 30, 5.0, 600, "
            "'2026-01-01 10:00:00', '2026-01-01 10:00:00'), "
            "(2, 'O-NEW', 1, 2, 2, 'PENDING_PAYMENT', 100, 20, NULL, NULL, 10, 2.0, 240, "
            "'2026-01-02 10:00:00', '2026-01-02 10:00:00')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO prediction VALUES "
            "(1, 1, '2026-01-03 11:00:00', '1h', 0.80, 1, 'HIGH', 'demo', 0.1, 0.1, "
            "'2026-01-03 10:00:00', '2026-01-03 10:00:00'), "
            "(2, 2, '2026-01-03 11:00:00', '1h', 0.20, 1, 'LOW', 'demo', 0.1, 0.1, "
            "'2026-01-03 10:00:00', '2026-01-03 10:00:00')"));
    check(demoDataCreated, QStringLiteral("extended user backend demo data is created"));
    if (!demoDataCreated) {
        return;
    }

    QTemporaryDir avatarDirectory;
    check(avatarDirectory.isValid(), QStringLiteral("temporary avatar directory is available"));
    if (!avatarDirectory.isValid()) {
        return;
    }
    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserRepository userRepository;
    OrderRepository orderRepository;
    StationRepository stationRepository;
    PredictionRepository predictionRepository;
    UserService userService(&databaseManager, &userRepository, avatarDirectory.path());
    OrderService orderService(&databaseManager, &userRepository, &orderRepository);
    StationService stationService(&databaseManager, &stationRepository, &predictionRepository);
    UserHandler userHandler(&userService, &sessions);
    OrderHandler orderHandler(&orderService);
    StationHandler stationHandler(&stationService);
    registerUserHandlers(&dispatcher, &userHandler);
    registerOrderHandlers(&dispatcher, &orderHandler);
    registerStationHandlers(&dispatcher, &stationHandler);
    const QString sessionId = sessions.createSession(1, SessionRole::User);

    const QByteArray png = QByteArray("\x89PNG\r\n\x1a\n") + QByteArray("demo");
    RequestMessage avatarRequest{
        QStringLiteral("REQ-AVATAR"), MessageTypes::UserAvatarUpload, sessionId,
        {{QStringLiteral("fileName"), QStringLiteral("avatar.png")},
         {QStringLiteral("mimeType"), QStringLiteral("image/png")},
         {QStringLiteral("contentBase64"), QString::fromLatin1(png.toBase64())}}
    };
    const ResponseMessage avatarResponse = dispatcher.dispatch(avatarRequest);
    const QString avatarPath = avatarResponse.data.value(QStringLiteral("avatarPath")).toString();
    check(avatarResponse.code == ErrorCodes::Success
              && avatarPath.startsWith(QStringLiteral("avatars/"))
              && QFileInfo::exists(avatarDirectory.filePath(QFileInfo(avatarPath).fileName())),
          QStringLiteral("avatar upload validates content, stores file and returns relative path"));

    RequestMessage rechargeRequest{
        QStringLiteral("REQ-RECHARGE"), MessageTypes::UserRecharge, sessionId,
        {{QStringLiteral("amountFen"), 500}}
    };
    const ResponseMessage rechargeResponse = dispatcher.dispatch(rechargeRequest);
    const ResponseMessage duplicateRechargeResponse = dispatcher.dispatch(rechargeRequest);
    QSqlQuery rechargeCount(database);
    rechargeCount.exec(QStringLiteral("SELECT COUNT(*), balance_after_fen FROM recharge_record"));
    rechargeCount.next();
    check(rechargeResponse.code == ErrorCodes::Success
              && rechargeResponse.data.value(QStringLiteral("balanceFen")).toInt() == 1500
              && duplicateRechargeResponse.data.value(QStringLiteral("balanceFen")).toInt() == 1500
              && rechargeCount.value(0).toInt() == 1 && rechargeCount.value(1).toInt() == 1500,
          QStringLiteral("recharge updates balance and record atomically without duplicate request"));

    RequestMessage orderListRequest{
        QStringLiteral("REQ-ORDER-LIST"), MessageTypes::UserOrderList, sessionId,
        {{QStringLiteral("page"), 1}, {QStringLiteral("pageSize"), 1}}
    };
    const ResponseMessage orderListResponse = dispatcher.dispatch(orderListRequest);
    const QJsonArray items = orderListResponse.data.value(QStringLiteral("items")).toArray();
    check(orderListResponse.code == ErrorCodes::Success
              && orderListResponse.data.value(QStringLiteral("total")).toInt() == 2
              && items.size() == 1
              && items.first().toObject().value(QStringLiteral("orderNo")).toString()
                    == QStringLiteral("O-NEW"),
          QStringLiteral("order list paginates current user orders by newest first"));

    RequestMessage recommendationRequest{
        QStringLiteral("REQ-RECOMMEND"), MessageTypes::PredictionRecommendation, sessionId,
        {{QStringLiteral("longitude"), 116.397}, {QStringLiteral("latitude"), 39.908},
         {QStringLiteral("limit"), 2}, {QStringLiteral("horizon"), QStringLiteral("1h")}}
    };
    const ResponseMessage recommendationResponse = dispatcher.dispatch(recommendationRequest);
    const QJsonArray recommended = recommendationResponse.data.value(
        QStringLiteral("stations")).toArray();
    check(recommendationResponse.code == ErrorCodes::Success && recommended.size() == 2
              && recommended.first().toObject().value(QStringLiteral("stationId")).toInt() == 2
              && recommended.first().toObject().value(QStringLiteral("recommended")).toBool(),
          QStringLiteral("recommendations rank latest lower-load station ahead of nearer high-load station"));
}

void testStationListAndDetailFlow()
{
    // 独立内存库模拟三个站点，其中一个停用，不应暴露给普通用户。
    DatabaseManager databaseManager{
        QString::fromLatin1(":memory:"),
        QString::fromLatin1("station-backend-test")
    };
    QSqlDatabase database;
    QString databaseError;
    check(databaseManager.database(&database, &databaseError),
          QStringLiteral("station backend test database opens"));
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery schema(database);
    const bool stationTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_station ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, station_no TEXT NOT NULL, "
        "name TEXT NOT NULL, address TEXT NOT NULL, district TEXT, "
        "longitude REAL NOT NULL, latitude REAL NOT NULL, "
        "price_fen_per_kwh INTEGER NOT NULL, "
        "service_fee_fen_per_kwh INTEGER NOT NULL, status TEXT NOT NULL)"));
    const bool pileTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_pile ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, station_id INTEGER NOT NULL, "
        "pile_no TEXT NOT NULL, type TEXT NOT NULL, power_kw REAL NOT NULL, "
        "status TEXT NOT NULL)"));
    check(stationTableCreated && pileTableCreated,
          QStringLiteral("station backend test schema is created"));
    if (!stationTableCreated || !pileTableCreated) {
        return;
    }

    const bool demoDataCreated = schema.exec(QStringLiteral(
        "INSERT INTO charging_station "
        "(id, station_no, name, address, district, longitude, latitude, "
        "price_fen_per_kwh, service_fee_fen_per_kwh, status) VALUES "
        "(1, 'S001', '近处站点', '北京市东城区', '东城区', 116.397, 39.908, 120, 30, 'NORMAL'), "
        "(2, 'S002', '较远站点', '北京市朝阳区', '朝阳区', 116.507, 39.908, 130, 20, 'NORMAL'), "
        "(3, 'S003', '停用站点', '北京市东城区', '东城区', 116.398, 39.909, 100, 0, 'DISABLED')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_pile "
            "(station_id, pile_no, type, power_kw, status) VALUES "
            "(1, 'A-01', 'FAST', 120, 'AVAILABLE'), "
            "(1, 'A-02', 'SLOW', 7, 'CHARGING'), "
            "(2, 'B-01', 'FAST', 60, 'AVAILABLE'), "
            "(3, 'C-01', 'FAST', 60, 'AVAILABLE')"));
    check(demoDataCreated, QStringLiteral("station backend test demo data is created"));
    if (!demoDataCreated) {
        return;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    StationRepository stationRepository;
    StationService stationService(&databaseManager, &stationRepository);
    StationHandler stationHandler(&stationService);
    registerStationHandlers(&dispatcher, &stationHandler);
    const QString userSession = sessions.createSession(7, SessionRole::User);

    RequestMessage listRequest{
        QStringLiteral("REQ-STATION-LIST"),
        MessageTypes::StationListNearby,
        userSession,
        {{QStringLiteral("longitude"), 116.397},
         {QStringLiteral("latitude"), 39.908},
         {QStringLiteral("limit"), 20}}
    };
    const ResponseMessage listResponse = dispatcher.dispatch(listRequest);
    const QJsonArray stations = listResponse.data.value(QStringLiteral("stations")).toArray();
    const QJsonObject nearestStation = stations.isEmpty() ? QJsonObject{} : stations.first().toObject();
    check(listResponse.code == ErrorCodes::Success && stations.size() == 2
              && nearestStation.value(QStringLiteral("stationId")).toInt() == 1
              && nearestStation.value(QStringLiteral("pileCount")).toInt() == 2
              && nearestStation.value(QStringLiteral("availablePileCount")).toInt() == 1,
          QStringLiteral("nearby list hides disabled stations and aggregates available piles"));

    RequestMessage detailRequest{
        QStringLiteral("REQ-STATION-DETAIL"),
        MessageTypes::StationDetailGet,
        userSession,
        {{QStringLiteral("stationId"), 1}}
    };
    const ResponseMessage detailResponse = dispatcher.dispatch(detailRequest);
    const QJsonArray piles = detailResponse.data.value(QStringLiteral("piles")).toArray();
    check(detailResponse.code == ErrorCodes::Success
              && detailResponse.data.value(QStringLiteral("station")).toObject()
                     .value(QStringLiteral("name")).toString() == QStringLiteral("近处站点")
              && piles.size() == 2
              && piles.first().toObject().value(QStringLiteral("status")).toString()
                    == QStringLiteral("AVAILABLE"),
          QStringLiteral("station detail returns station and all pile states"));

    detailRequest.requestId = QStringLiteral("REQ-DISABLED-STATION");
    detailRequest.payload.insert(QStringLiteral("stationId"), 3);
    check(dispatcher.dispatch(detailRequest).code == ErrorCodes::StationNotFound,
          QStringLiteral("disabled station is hidden from normal users"));
}

void testOrderCreateAndActiveCheckFlow()
{
    // 订单创建必须同时改变订单和电桩，本测试用内存库验证该原子业务规则。
    DatabaseManager databaseManager{
        QString::fromLatin1(":memory:"),
        QString::fromLatin1("order-backend-test")
    };
    QSqlDatabase database;
    QString databaseError;
    check(databaseManager.database(&database, &databaseError),
          QStringLiteral("order backend test database opens"));
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery schema(database);
    const bool userTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE user (id INTEGER PRIMARY KEY, phone TEXT, nickname TEXT, "
        "avatar_path TEXT, balance_fen INTEGER, status TEXT, created_at TEXT, "
        "updated_at TEXT)"));
    const bool stationTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_station (id INTEGER PRIMARY KEY, name TEXT, "
        "status TEXT, price_fen_per_kwh INTEGER, service_fee_fen_per_kwh INTEGER)"));
    const bool pileTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_pile (id INTEGER PRIMARY KEY, station_id INTEGER, "
        "pile_no TEXT, power_kw REAL, status TEXT, current_order_id INTEGER, "
        "total_charge_count INTEGER DEFAULT 0, total_charge_minutes INTEGER DEFAULT 0, "
        "total_energy_kwh REAL DEFAULT 0, updated_at TEXT)"));
    const bool orderTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_order ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, order_no TEXT, user_id INTEGER, "
        "station_id INTEGER, pile_id INTEGER, status TEXT, price_fen_per_kwh INTEGER, "
        "service_fee_fen_per_kwh INTEGER, start_at TEXT, end_at TEXT, "
        "charge_minutes INTEGER DEFAULT 0, energy_kwh REAL DEFAULT 0, "
        "amount_fen INTEGER DEFAULT 0, paid_at TEXT, cancelled_at TEXT, "
        "cancel_reason TEXT, created_at TEXT, updated_at TEXT)"));
    check(userTableCreated && stationTableCreated && pileTableCreated && orderTableCreated,
          QStringLiteral("order backend test schema is created"));
    if (!userTableCreated || !stationTableCreated || !pileTableCreated || !orderTableCreated) {
        return;
    }

    const bool demoDataCreated = schema.exec(QStringLiteral(
        "INSERT INTO user (id, phone, nickname, balance_fen, status, created_at) VALUES "
        "(1, '13800138001', '用户一', 5000, 'NORMAL', '2026-01-01 00:00:00'), "
        "(2, '13800138002', '用户二', 5000, 'NORMAL', '2026-01-01 00:00:00'), "
        "(3, '13800138003', '冻结用户', 5000, 'FROZEN', '2026-01-01 00:00:00')"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_station "
            "(id, name, status, price_fen_per_kwh, service_fee_fen_per_kwh) "
            "VALUES (1, '演示站点', 'NORMAL', 120, 30)"))
        && schema.exec(QStringLiteral(
            "INSERT INTO charging_pile (id, station_id, pile_no, power_kw, status) VALUES "
            "(1, 1, 'A-01', 60, 'AVAILABLE'), (2, 1, 'A-02', 60, 'AVAILABLE')"));
    check(demoDataCreated, QStringLiteral("order backend test demo data is created"));
    if (!demoDataCreated) {
        return;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserRepository userRepository;
    OrderRepository orderRepository;
    OrderService orderService(&databaseManager, &userRepository, &orderRepository);
    OrderHandler orderHandler(&orderService);
    registerOrderHandlers(&dispatcher, &orderHandler);
    const QString userOneSession = sessions.createSession(1, SessionRole::User);
    const QString userTwoSession = sessions.createSession(2, SessionRole::User);
    const QString frozenUserSession = sessions.createSession(3, SessionRole::User);

    RequestMessage createRequest{
        QStringLiteral("REQ-ORDER-CREATE"), MessageTypes::OrderCreate, userOneSession,
        {{QStringLiteral("pileId"), 1}}
    };
    const ResponseMessage createResponse = dispatcher.dispatch(createRequest);
    const QJsonObject createdOrder = createResponse.data.value(QStringLiteral("order")).toObject();
    const qint64 createdOrderId = static_cast<qint64>(
        createdOrder.value(QStringLiteral("orderId")).toDouble());
    QSqlQuery pileCheck(database);
    pileCheck.exec(QStringLiteral("SELECT status, current_order_id FROM charging_pile WHERE id = 1"));
    pileCheck.next();
    check(createResponse.code == ErrorCodes::Success
              && createdOrder.value(QStringLiteral("status")).toString()
                    == QStringLiteral("CREATED")
              && createdOrder.value(QStringLiteral("priceFenPerKwh")).toInt() == 120
              && pileCheck.value(0).toString() == QStringLiteral("RESERVED")
              && pileCheck.value(1).toLongLong() == createdOrderId,
          QStringLiteral("create order atomically reserves available pile with price snapshot"));

    createRequest.requestId = QStringLiteral("REQ-DUPLICATE-ORDER");
    check(dispatcher.dispatch(createRequest).code == ErrorCodes::ActiveOrderExists,
          QStringLiteral("user cannot create a second active order"));

    RequestMessage activeRequest{
        QStringLiteral("REQ-ACTIVE-ORDER"), MessageTypes::OrderActiveCheck, userOneSession, {}
    };
    const ResponseMessage activeResponse = dispatcher.dispatch(activeRequest);
    check(activeResponse.code == ErrorCodes::Success
              && activeResponse.data.value(QStringLiteral("hasActiveOrder")).toBool()
              && activeResponse.data.value(QStringLiteral("balanceFen")).toInt() == 5000
              && activeResponse.data.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("orderId")).toDouble() == createdOrderId,
          QStringLiteral("active order check returns current order and wallet balance"));

    createRequest.requestId = QStringLiteral("REQ-OCCUPIED-PILE");
    createRequest.sessionId = userTwoSession;
    check(dispatcher.dispatch(createRequest).code == ErrorCodes::PileUnavailable,
          QStringLiteral("another user cannot reserve the occupied pile"));

    createRequest.requestId = QStringLiteral("REQ-FROZEN-USER");
    createRequest.sessionId = frozenUserSession;
    createRequest.payload.insert(QStringLiteral("pileId"), 2);
    check(dispatcher.dispatch(createRequest).code == ErrorCodes::UserFrozen,
          QStringLiteral("frozen user cannot create a new order"));

    RequestMessage startRequest{
        QStringLiteral("REQ-ORDER-START"), MessageTypes::OrderStart, userOneSession,
        {{QStringLiteral("orderId"), createdOrderId}}
    };
    const ResponseMessage startResponse = dispatcher.dispatch(startRequest);
    pileCheck.exec(QStringLiteral("SELECT status FROM charging_pile WHERE id = 1"));
    pileCheck.next();
    check(startResponse.code == ErrorCodes::Success
              && startResponse.data.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("status")).toString() == QStringLiteral("CHARGING")
              && pileCheck.value(0).toString() == QStringLiteral("CHARGING"),
          QStringLiteral("start order changes both order and reserved pile to charging"));

    const QString simulatedStart = QDateTime::currentDateTime().addSecs(-120)
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    schema.prepare(QStringLiteral(
        "UPDATE charging_order SET start_at = :startAt WHERE id = :orderId"));
    schema.bindValue(QStringLiteral(":startAt"), simulatedStart);
    schema.bindValue(QStringLiteral(":orderId"), createdOrderId);
    check(schema.exec(), QStringLiteral("test can set deterministic charging duration"));

    RequestMessage stopRequest{
        QStringLiteral("REQ-ORDER-STOP"), MessageTypes::OrderStop, userOneSession,
        {{QStringLiteral("orderId"), createdOrderId}}
    };
    const ResponseMessage stopResponse = dispatcher.dispatch(stopRequest);
    const QJsonObject stoppedOrder = stopResponse.data.value(QStringLiteral("order")).toObject();
    pileCheck.exec(QStringLiteral("SELECT status, current_order_id FROM charging_pile WHERE id = 1"));
    pileCheck.next();
    check(stopResponse.code == ErrorCodes::Success
              && stoppedOrder.value(QStringLiteral("status")).toString()
                    == QStringLiteral("PENDING_PAYMENT")
              && stoppedOrder.value(QStringLiteral("energyKwh")).toDouble() > 0.0
              && stoppedOrder.value(QStringLiteral("amountFen")).toInt() > 0
              && pileCheck.value(0).toString() == QStringLiteral("AVAILABLE")
              && pileCheck.value(1).isNull(),
          QStringLiteral("stop order calculates amount and releases charging pile"));

    RequestMessage settleRequest{
        QStringLiteral("REQ-ORDER-SETTLE"), MessageTypes::OrderSettle, userOneSession,
        {{QStringLiteral("orderId"), createdOrderId}}
    };
    const ResponseMessage settleResponse = dispatcher.dispatch(settleRequest);
    const qint64 balanceAfterSettlement = static_cast<qint64>(
        settleResponse.data.value(QStringLiteral("balanceFen")).toDouble());
    pileCheck.exec(QStringLiteral(
        "SELECT total_charge_count, total_charge_minutes, total_energy_kwh "
        "FROM charging_pile WHERE id = 1"));
    pileCheck.next();
    check(settleResponse.code == ErrorCodes::Success
              && settleResponse.data.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("status")).toString() == QStringLiteral("COMPLETED")
              && balanceAfterSettlement < 5000
              && pileCheck.value(0).toInt() == 1
              && pileCheck.value(1).toInt() > 0
              && pileCheck.value(2).toDouble() > 0.0,
          QStringLiteral("settlement deducts balance and accumulates pile statistics once"));

    const ResponseMessage repeatedSettleResponse = dispatcher.dispatch(settleRequest);
    check(repeatedSettleResponse.code == ErrorCodes::Success
              && repeatedSettleResponse.data.value(QStringLiteral("balanceFen")).toDouble()
                    == balanceAfterSettlement,
          QStringLiteral("repeated settlement is idempotent and does not deduct twice"));

    createRequest.requestId = QStringLiteral("REQ-CANCEL-CREATE");
    createRequest.sessionId = userOneSession;
    createRequest.payload.insert(QStringLiteral("pileId"), 1);
    const ResponseMessage secondCreateResponse = dispatcher.dispatch(createRequest);
    const qint64 secondOrderId = static_cast<qint64>(
        secondCreateResponse.data.value(QStringLiteral("order")).toObject()
            .value(QStringLiteral("orderId")).toDouble());
    RequestMessage cancelRequest{
        QStringLiteral("REQ-ORDER-CANCEL"), MessageTypes::OrderCancel, userOneSession,
        {{QStringLiteral("orderId"), secondOrderId},
         {QStringLiteral("reason"), QStringLiteral("change of plans")}}
    };
    const ResponseMessage cancelResponse = dispatcher.dispatch(cancelRequest);
    pileCheck.exec(QStringLiteral("SELECT status, current_order_id FROM charging_pile WHERE id = 1"));
    pileCheck.next();
    check(secondCreateResponse.code == ErrorCodes::Success
              && cancelResponse.code == ErrorCodes::Success
              && cancelResponse.data.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("status")).toString() == QStringLiteral("CANCELLED")
              && pileCheck.value(0).toString() == QStringLiteral("AVAILABLE")
              && pileCheck.value(1).isNull(),
          QStringLiteral("cancelling created order releases its reserved pile"));
    check(dispatcher.dispatch(cancelRequest).code == ErrorCodes::OrderCannotCancel,
          QStringLiteral("only created orders can be cancelled"));
}

void testUserLoginOverTcp()
{
    // 该测试覆盖真实字节流：SocketClient -> SocketServer -> Dispatcher -> SQLite。
    DatabaseManager databaseManager{
        QString::fromLatin1(":memory:"),
        QString::fromLatin1("user-backend-tcp-test")
    };
    QSqlDatabase database;
    QString databaseError;
    check(databaseManager.database(&database, &databaseError),
          QStringLiteral("TCP user backend test database opens"));
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery schema(database);
    const bool userTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE user ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "phone TEXT NOT NULL UNIQUE, nickname TEXT NOT NULL, "
        "avatar_path TEXT, balance_fen INTEGER NOT NULL DEFAULT 0, "
        "status TEXT NOT NULL DEFAULT 'NORMAL', last_login_at TEXT, "
        "created_at TEXT NOT NULL, updated_at TEXT NOT NULL)"));
    const bool orderTableCreated = schema.exec(QStringLiteral(
        "CREATE TABLE charging_order ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER NOT NULL, "
        "status TEXT NOT NULL)"));
    check(userTableCreated && orderTableCreated,
          QStringLiteral("TCP user backend test schema is created"));
    if (!userTableCreated || !orderTableCreated) {
        return;
    }

    SessionManager sessions;
    MessageDispatcher dispatcher(&sessions);
    UserRepository userRepository;
    UserService userService(&databaseManager, &userRepository);
    UserHandler userHandler(&userService, &sessions);
    registerUserHandlers(&dispatcher, &userHandler);

    SocketServer server(&dispatcher);
    check(server.listen(QHostAddress::LocalHost, 0) && server.serverPort() != 0,
          QStringLiteral("application SocketServer listens on an ephemeral port"));
    if (server.serverPort() == 0) {
        return;
    }

    SocketClient client;
    QEventLoop connectedLoop;
    QObject::connect(&client, &SocketClient::connected,
                     &connectedLoop, &QEventLoop::quit);
    client.connectToServer(QStringLiteral("127.0.0.1"), server.serverPort());
    QTimer::singleShot(1000, &connectedLoop, &QEventLoop::quit);
    connectedLoop.exec();
    check(client.isConnected(),
          QStringLiteral("SocketClient connects to application SocketServer"));
    if (!client.isConnected()) {
        return;
    }

    QJsonObject response;
    QEventLoop responseLoop;
    QObject::connect(&client, &SocketClient::responseReceived,
                     &responseLoop, [&](const QJsonObject &received) {
        response = received;
        responseLoop.quit();
    });
    const QString requestId = client.sendRequest(
        MessageTypes::UserLogin, {},
        {{QStringLiteral("phone"), QStringLiteral("13800138001")}},
        QStringLiteral("REQ-TCP-USER-LOGIN"), 1000);
    QTimer::singleShot(1500, &responseLoop, &QEventLoop::quit);
    responseLoop.exec();
    check(requestId == QStringLiteral("REQ-TCP-USER-LOGIN")
              && response.value(QStringLiteral("requestId")).toString() == requestId
              && response.value(QStringLiteral("code")).toInt() == ErrorCodes::Success
              && !response.value(QStringLiteral("data")).toObject()
                      .value(QStringLiteral("sessionId")).toString().isEmpty(),
          QStringLiteral("TCP login reaches user handler and returns a session"));
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
    testUserLoginProfileAndNicknameFlow();
    testUserProfileWalletOrdersAndRecommendations();
    testStationListAndDetailFlow();
    testOrderCreateAndActiveCheckFlow();
    testUserLoginOverTcp();
    testUserClientRequestTimeout();
    testAdminClientRequestTimeout();
    // 非零退出码可直接用于CI或提交前检查。
    QTextStream(stdout) << "TOTAL_FAILURES=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
