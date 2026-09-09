#include "services/admin/adminmanagementservice.h"
#include "database/databasemanager.h"
#include "shared/protocol/errorcodes.h"
#include "devices/devicecontrolservice.h"
#include "devices/deviceregistry.h"
#include "devices/devicesession.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

class AdminManagementTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void freezeAndUnfreezeAreIdempotent();
    void missingUserReturnsDocumentedError();
    void invalidStationPriceIsRejected();
    void createStationAndListPiles();
    void restartAvailablePile();
    void listOrdersForAdmin();
    void issueCouponForUser();
    void chargingFaultStopsOrderAndWritesSystemLog();
    void reservedFaultCancelsOrder();
    void chargingOfflineIsIdempotent();
    void reconnectRestoresOnlyUnoccupiedOfflinePile();
    void restartCorrectAckCompletesOnce();
    void restartStaleAckIsIgnored();

private:
    RequestMessage request(qint64 userId) const;
    QSqlDatabase m_database;
    QTemporaryDir m_temporaryDirectory;
    DatabaseManager *m_databaseManager = nullptr;
    AdminManagementService *m_service = nullptr;
    DeviceRegistry *m_devices = nullptr;
    DeviceControlService *m_deviceControl = nullptr;
};

void AdminManagementTest::initTestCase()
{
    QVERIFY(m_temporaryDirectory.isValid());
    m_databaseManager = new DatabaseManager(
        m_temporaryDirectory.filePath(QStringLiteral("admin-test.db")),
        QStringLiteral("admin-test"));
    QString error;
    QVERIFY2(m_databaseManager->database(&m_database, &error), qPrintable(error));
    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE user(id INTEGER PRIMARY KEY, phone TEXT, nickname TEXT, balance_fen INTEGER, "
        "status TEXT, created_at TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE operation_log(id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, "
        "action TEXT, target_type TEXT, target_id INTEGER, before_status TEXT, "
        "after_status TEXT, result TEXT, message TEXT, created_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE charging_station(id INTEGER PRIMARY KEY AUTOINCREMENT, station_no TEXT, "
        "name TEXT, address TEXT, longitude REAL, latitude REAL, price_fen_per_kwh INTEGER, "
        "service_fee_fen_per_kwh INTEGER, status TEXT, created_at TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE charging_pile(id INTEGER PRIMARY KEY AUTOINCREMENT, station_id INTEGER, "
        "pile_no TEXT, type TEXT, power_kw REAL, status TEXT, current_order_id INTEGER, last_heartbeat_at TEXT, total_charge_count INTEGER DEFAULT 0, "
        "total_charge_minutes INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE charging_order(id INTEGER PRIMARY KEY AUTOINCREMENT, order_no TEXT, "
        "user_id INTEGER, station_id INTEGER, pile_id INTEGER, status TEXT, "
        "price_fen_per_kwh INTEGER, service_fee_fen_per_kwh INTEGER, start_at TEXT, end_at TEXT, "
        "charge_minutes INTEGER DEFAULT 0, energy_kwh REAL DEFAULT 0, amount_fen INTEGER DEFAULT 0, cancelled_at TEXT, cancel_reason TEXT, "
        "created_at TEXT, updated_at TEXT, coupon_id INTEGER, discount_rate INTEGER DEFAULT 100)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE coupon(id INTEGER PRIMARY KEY AUTOINCREMENT, user_id INTEGER, "
        "discount_rate INTEGER DEFAULT 80, status TEXT DEFAULT 'AVAILABLE', order_id INTEGER, "
        "issued_by INTEGER, issued_at TEXT, used_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO user VALUES(1, '13800138000', '测试用户', 10000, 'NORMAL', "
        "'2026-09-02 00:00:00', '2026-09-02 00:00:00')")));
    m_service = new AdminManagementService(m_databaseManager, this);
    m_devices = new DeviceRegistry(this);
    m_deviceControl = new DeviceControlService(m_databaseManager, m_devices, this);
}

void AdminManagementTest::cleanupTestCase()
{
    delete m_service;
    delete m_deviceControl;
    delete m_devices;
    delete m_databaseManager;
    m_databaseManager = nullptr;
    m_database.close();
    m_database = {};
}

static qint64 addDeviceOrder(QSqlDatabase &db, const QString &pileStatus, const QString &orderStatus,
                             const QString &startAt = QStringLiteral("2026-09-09 10:00:00"))
{
    QSqlQuery q(db); q.exec(QStringLiteral("INSERT INTO charging_station(station_no,name,address,longitude,latitude,price_fen_per_kwh,service_fee_fen_per_kwh,status,created_at,updated_at) VALUES('DEV','D','D',1,1,100,0,'NORMAL','2026-01-01 00:00:00','2026-01-01 00:00:00')"));
    const qint64 station=q.lastInsertId().toLongLong(); q.prepare(QStringLiteral("INSERT INTO charging_pile(station_id,pile_no,type,power_kw,status,created_at,updated_at) VALUES(:s,'DEV-P','FAST',60,:status,'2026-01-01 00:00:00','2026-01-01 00:00:00')"));q.bindValue(QStringLiteral(":s"),station);q.bindValue(QStringLiteral(":status"),pileStatus);q.exec();const qint64 pile=q.lastInsertId().toLongLong();
    q.prepare(QStringLiteral("INSERT INTO charging_order(order_no,user_id,station_id,pile_id,status,price_fen_per_kwh,service_fee_fen_per_kwh,start_at,created_at,updated_at) VALUES(:no,1,:s,:p,:status,100,0,:start,'2026-01-01 00:00:00','2026-01-01 00:00:00')"));q.bindValue(QStringLiteral(":no"),QStringLiteral("DEV-%1").arg(pile));q.bindValue(QStringLiteral(":s"),station);q.bindValue(QStringLiteral(":p"),pile);q.bindValue(QStringLiteral(":status"),orderStatus);q.bindValue(QStringLiteral(":start"),startAt);q.exec();const qint64 order=q.lastInsertId().toLongLong();q.prepare(QStringLiteral("UPDATE charging_pile SET current_order_id=:o WHERE id=:p"));q.bindValue(QStringLiteral(":o"),order);q.bindValue(QStringLiteral(":p"),pile);q.exec();return pile;
}

void AdminManagementTest::chargingFaultStopsOrderAndWritesSystemLog()
{ const qint64 pile=addDeviceOrder(m_database,QStringLiteral("CHARGING"),QStringLiteral("CHARGING"));m_deviceControl->handleFault(pile,QStringLiteral("TEMP_HIGH"),QStringLiteral("hot"));QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT o.status,o.energy_kwh,o.amount_fen,p.status,p.current_order_id FROM charging_order o JOIN charging_pile p ON p.id=o.pile_id WHERE p.id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("PENDING_PAYMENT"));QVERIFY(q.value(1).toDouble()>0);QVERIFY(q.value(2).toLongLong()>0);QCOMPARE(q.value(3).toString(),QStringLiteral("FAULT"));QVERIFY(q.value(4).isNull());q.prepare(QStringLiteral("SELECT admin_id FROM operation_log WHERE target_id=:p AND action='DEVICE_FAULT'"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QVERIFY(q.value(0).isNull()); }
void AdminManagementTest::reservedFaultCancelsOrder()
{ const qint64 pile=addDeviceOrder(m_database,QStringLiteral("RESERVED"),QStringLiteral("CREATED"));m_deviceControl->handleFault(pile,QStringLiteral("TEMP_HIGH"),QStringLiteral("hot"));QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT o.status,o.cancel_reason,p.status,p.current_order_id FROM charging_order o JOIN charging_pile p ON p.id=o.pile_id WHERE p.id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("CANCELLED"));QCOMPARE(q.value(1).toString(),QStringLiteral("设备故障自动取消"));QCOMPARE(q.value(2).toString(),QStringLiteral("FAULT"));QVERIFY(q.value(3).isNull()); }
void AdminManagementTest::chargingOfflineIsIdempotent()
{ const qint64 pile=addDeviceOrder(m_database,QStringLiteral("CHARGING"),QStringLiteral("CHARGING"));m_deviceControl->handleOffline(pile);m_deviceControl->handleOffline(pile);QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT o.status,o.energy_kwh,p.status FROM charging_order o JOIN charging_pile p ON p.id=o.pile_id WHERE p.id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("PENDING_PAYMENT"));QVERIFY(q.value(1).toDouble()>0);QCOMPARE(q.value(2).toString(),QStringLiteral("OFFLINE")); }
void AdminManagementTest::reconnectRestoresOnlyUnoccupiedOfflinePile()
{ QSqlQuery q(m_database);q.exec(QStringLiteral("INSERT INTO charging_station(station_no,name,address,longitude,latitude,price_fen_per_kwh,service_fee_fen_per_kwh,status,created_at,updated_at) VALUES('REC','R','R',1,1,100,0,'NORMAL','2026-01-01 00:00:00','2026-01-01 00:00:00')"));const qint64 station=q.lastInsertId().toLongLong();q.prepare(QStringLiteral("INSERT INTO charging_pile(station_id,pile_no,type,power_kw,status,created_at,updated_at) VALUES(:s,'REC-P','FAST',60,'OFFLINE','2026-01-01 00:00:00','2026-01-01 00:00:00')"));q.bindValue(QStringLiteral(":s"),station);QVERIFY(q.exec());const qint64 pile=q.lastInsertId().toLongLong();double power=0;QString status;QVERIFY(m_deviceControl->completeHello(pile,QStringLiteral("REC-P"),&power,&status));QCOMPARE(status,QStringLiteral("AVAILABLE"));QVERIFY(m_deviceControl->validateHello(pile,QStringLiteral("REC-P"),&power,&status));q.prepare(QStringLiteral("UPDATE charging_pile SET status='FAULT' WHERE id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(m_deviceControl->completeHello(pile,QStringLiteral("REC-P"),&power,&status));QCOMPARE(status,QStringLiteral("FAULT")); }

RequestMessage AdminManagementTest::request(qint64 userId) const
{
    return {QStringLiteral("TEST-1"), QStringLiteral("ADMIN_USER_FREEZE"),
            QStringLiteral("S-ADMIN"), {{QStringLiteral("userId"), userId}}};
}

static qint64 addRestartPile(QSqlDatabase &db)
{ QSqlQuery q(db);q.exec(QStringLiteral("INSERT INTO charging_station(station_no,name,address,longitude,latitude,price_fen_per_kwh,service_fee_fen_per_kwh,status,created_at,updated_at) VALUES('RST','R','R',1,1,100,0,'NORMAL','2026-01-01 00:00:00','2026-01-01 00:00:00')"));const qint64 s=q.lastInsertId().toLongLong();q.prepare(QStringLiteral("INSERT INTO charging_pile(station_id,pile_no,type,power_kw,status,created_at,updated_at) VALUES(:s,'RST-P','FAST',60,'RESTARTING','2026-01-01 00:00:00','2026-01-01 00:00:00')"));q.bindValue(QStringLiteral(":s"),s);q.exec();return q.lastInsertId().toLongLong(); }
static QTcpSocket *attachDevice(QTcpServer &server, DeviceRegistry *registry, DeviceControlService *control, DatabaseManager *db, qint64 pile)
{ if(!server.listen(QHostAddress::LocalHost))return nullptr;auto *client=new QTcpSocket(&server);client->connectToHost(QHostAddress::LocalHost,server.serverPort());if(!client->waitForConnected()||!server.waitForNewConnection(1000))return nullptr;auto *session=new DeviceSession(server.nextPendingConnection(),db,registry,control,&server);registry->registerSession(pile,session);return client; }
void AdminManagementTest::restartCorrectAckCompletesOnce()
{ const qint64 pile=addRestartPile(m_database);QTcpServer server;QTcpSocket *client=attachDevice(server,m_devices,m_deviceControl,m_databaseManager,pile);QVERIFY(client);QSignalSpy finished(m_deviceControl,&DeviceControlService::restartFinished);QVERIFY(m_deviceControl->restart(pile,9,QStringLiteral("FAULT")));QTRY_VERIFY(client->canReadLine());const QJsonObject command=QJsonDocument::fromJson(client->readLine()).object();m_deviceControl->handleAck(pile,{{QStringLiteral("command"),QStringLiteral("RESTART")},{QStringLiteral("commandId"),command.value(QStringLiteral("commandId"))},{QStringLiteral("success"),true}});QTRY_COMPARE(finished.count(),1);m_deviceControl->handleAck(pile,{{QStringLiteral("command"),QStringLiteral("RESTART")},{QStringLiteral("commandId"),command.value(QStringLiteral("commandId"))},{QStringLiteral("success"),true}});QCOMPARE(finished.count(),1);QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("AVAILABLE")); }
void AdminManagementTest::restartStaleAckIsIgnored()
{ const qint64 pile=addRestartPile(m_database);QTcpServer server;QTcpSocket *client=attachDevice(server,m_devices,m_deviceControl,m_databaseManager,pile);QSignalSpy finished(m_deviceControl,&DeviceControlService::restartFinished);QVERIFY(m_deviceControl->restart(pile,9,QStringLiteral("FAULT")));QTRY_VERIFY(client->canReadLine());client->readLine();m_deviceControl->handleAck(pile,{{QStringLiteral("command"),QStringLiteral("RESTART")},{QStringLiteral("commandId"),QStringLiteral("stale")},{QStringLiteral("success"),true}});QCOMPARE(finished.count(),0);QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id=:p"));q.bindValue(QStringLiteral(":p"),pile);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("RESTARTING")); }

void AdminManagementTest::freezeAndUnfreezeAreIdempotent()
{
    ResponseMessage response = m_service->setUserFrozen(request(1), 9, true);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("changed")).toBool(), true);
    QCOMPARE(response.data.value(QStringLiteral("status")).toString(), QStringLiteral("FROZEN"));

    response = m_service->setUserFrozen(request(1), 9, true);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("changed")).toBool(), false);

    response = m_service->setUserFrozen(request(1), 9, false);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("status")).toString(), QStringLiteral("NORMAL"));
    response = m_service->setUserFrozen(request(1), 9, false);
    QCOMPARE(response.data.value(QStringLiteral("changed")).toBool(), false);

    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM operation_log")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);
}

void AdminManagementTest::missingUserReturnsDocumentedError()
{
    const ResponseMessage response = m_service->setUserFrozen(request(999), 9, true);
    QCOMPARE(response.code, ErrorCodes::InvalidPhone);
}

void AdminManagementTest::invalidStationPriceIsRejected()
{
    const RequestMessage invalid{
        QStringLiteral("TEST-STATION"), QStringLiteral("ADMIN_STATION_CREATE"),
        QStringLiteral("S-ADMIN"),
        {{QStringLiteral("name"), QStringLiteral("测试站")},
         {QStringLiteral("address"), QStringLiteral("测试地址")},
         {QStringLiteral("longitude"), 121.5},
         {QStringLiteral("latitude"), 38.9},
         {QStringLiteral("pileCount"), 2},
         {QStringLiteral("priceFenPerKwh"), 0}}};
    const ResponseMessage response = m_service->createStation(invalid, 9);
    QCOMPARE(response.code, ErrorCodes::InvalidSocketMessage);
}

void AdminManagementTest::createStationAndListPiles()
{
    const RequestMessage create{
        QStringLiteral("TEST-CREATE"), QStringLiteral("ADMIN_STATION_CREATE"),
        QStringLiteral("S-ADMIN"),
        {{QStringLiteral("name"), QStringLiteral("测试站")},
         {QStringLiteral("address"), QStringLiteral("测试地址")},
         {QStringLiteral("longitude"), 121.5},
         {QStringLiteral("latitude"), 38.9},
         {QStringLiteral("pileCount"), 2},
         {QStringLiteral("priceFenPerKwh"), 135}}};
    const ResponseMessage created = m_service->createStation(create, 9);
    QCOMPARE(created.code, ErrorCodes::Success);
    QCOMPARE(created.data.value(QStringLiteral("pileCount")).toInt(), 2);

    const qint64 stationId = created.data.value(QStringLiteral("stationId")).toInteger();
    QVERIFY(stationId > 0);
    const RequestMessage list{
        QStringLiteral("TEST-LIST"), QStringLiteral("ADMIN_PILE_LIST"),
        QStringLiteral("S-ADMIN"), {{QStringLiteral("stationId"), stationId}}};
    const ResponseMessage listed = m_service->pileList(list);
    QCOMPARE(listed.code, ErrorCodes::Success);
    const QJsonArray piles = listed.data.value(QStringLiteral("piles")).toArray();
    QCOMPARE(piles.size(), 2);
    QCOMPARE(piles.first().toObject().value(QStringLiteral("stationId")).toInteger(), stationId);
    QVERIFY(!piles.first().toObject().value(QStringLiteral("type")).toString().isEmpty());

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT price_fen_per_kwh FROM charging_station WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), stationId);
    QVERIFY(query.exec()); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 135);
}

void AdminManagementTest::restartAvailablePile()
{
    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral("SELECT id FROM charging_pile ORDER BY id LIMIT 1")));
    QVERIFY(query.next());
    const qint64 pileId = query.value(0).toLongLong();
    const RequestMessage restart{
        QStringLiteral("TEST-RESTART"), QStringLiteral("ADMIN_PILE_RESTART"),
        QStringLiteral("S-ADMIN"), {{QStringLiteral("pileId"), pileId}}};
    const ResponseMessage response = m_service->restartPile(restart, 9);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("status")).toString(), QStringLiteral("RESTARTING"));

    query.prepare(QStringLiteral("SELECT status FROM charging_pile WHERE id=:id"));
    query.bindValue(QStringLiteral(":id"), pileId);
    QVERIFY(query.exec()); QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("RESTARTING"));
}

void AdminManagementTest::listOrdersForAdmin()
{
    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral("SELECT id FROM charging_station ORDER BY id LIMIT 1")));
    QVERIFY(query.next()); const qint64 stationId = query.value(0).toLongLong();
    QVERIFY(query.exec(QStringLiteral("SELECT id FROM charging_pile ORDER BY id LIMIT 1")));
    QVERIFY(query.next()); const qint64 pileId = query.value(0).toLongLong();
    query.prepare(QStringLiteral(
        "INSERT INTO charging_order(order_no,user_id,station_id,pile_id,status,"
        "price_fen_per_kwh,service_fee_fen_per_kwh,charge_minutes,energy_kwh,amount_fen,"
        "created_at,updated_at) VALUES('O-ADMIN-1',1,:station,:pile,'COMPLETED',"
        "135,0,30,20.5,2768,'2026-09-08 10:00:00','2026-09-08 10:30:00')"));
    query.bindValue(QStringLiteral(":station"), stationId);
    query.bindValue(QStringLiteral(":pile"), pileId);
    QVERIFY(query.exec());

    const RequestMessage request{
        QStringLiteral("TEST-ORDER-LIST"), QStringLiteral("ADMIN_ORDER_LIST"),
        QStringLiteral("S-ADMIN"),
        {{QStringLiteral("page"), 1}, {QStringLiteral("pageSize"), 20},
         {QStringLiteral("phoneKeyword"), QStringLiteral("1380")},
         {QStringLiteral("status"), QStringLiteral("COMPLETED")}}};
    const ResponseMessage response = m_service->orderList(request);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("total")).toInteger(), 1);
    const QJsonObject order = response.data.value(QStringLiteral("items"))
                                  .toArray().first().toObject();
    QCOMPARE(order.value(QStringLiteral("orderNo")).toString(), QStringLiteral("O-ADMIN-1"));
    QCOMPARE(order.value(QStringLiteral("userPhone")).toString(), QStringLiteral("13800138000"));
    QCOMPARE(order.value(QStringLiteral("userNickname")).toString(), QStringLiteral("测试用户"));
}

void AdminManagementTest::issueCouponForUser()
{
    const RequestMessage issue{
        QStringLiteral("TEST-COUPON-ISSUE"), QStringLiteral("ADMIN_COUPON_ISSUE"),
        QStringLiteral("S-ADMIN"), {{QStringLiteral("userId"), 1}}};
    const ResponseMessage response = m_service->issueCoupon(issue, 9);
    QCOMPARE(response.code, ErrorCodes::Success);
    QCOMPARE(response.data.value(QStringLiteral("discountRate")).toInt(), 80);
    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral(
        "SELECT user_id, discount_rate, status FROM coupon ORDER BY id DESC LIMIT 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), 1);
    QCOMPARE(query.value(1).toInt(), 80);
    QCOMPARE(query.value(2).toString(), QStringLiteral("AVAILABLE"));
}

QTEST_MAIN(AdminManagementTest)
#include "tst_adminmanagement.moc"
