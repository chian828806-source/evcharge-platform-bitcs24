#include "services/admin/adminmanagementservice.h"
#include "database/databasemanager.h"
#include "shared/protocol/errorcodes.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonArray>
#include <QTemporaryDir>
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

private:
    RequestMessage request(qint64 userId) const;
    QSqlDatabase m_database;
    QTemporaryDir m_temporaryDirectory;
    DatabaseManager *m_databaseManager = nullptr;
    AdminManagementService *m_service = nullptr;
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
        "pile_no TEXT, type TEXT, power_kw REAL, status TEXT, total_charge_count INTEGER DEFAULT 0, "
        "total_charge_minutes INTEGER DEFAULT 0, created_at TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE charging_order(id INTEGER PRIMARY KEY AUTOINCREMENT, order_no TEXT, "
        "user_id INTEGER, station_id INTEGER, pile_id INTEGER, status TEXT, "
        "price_fen_per_kwh INTEGER, service_fee_fen_per_kwh INTEGER, start_at TEXT, end_at TEXT, "
        "charge_minutes INTEGER DEFAULT 0, energy_kwh REAL DEFAULT 0, amount_fen INTEGER DEFAULT 0, "
        "created_at TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO user VALUES(1, '13800138000', '测试用户', 10000, 'NORMAL', "
        "'2026-09-02 00:00:00', '2026-09-02 00:00:00')")));
    m_service = new AdminManagementService(m_databaseManager, this);
}

void AdminManagementTest::cleanupTestCase()
{
    delete m_service;
    delete m_databaseManager;
    m_databaseManager = nullptr;
    m_database.close();
    m_database = {};
}

RequestMessage AdminManagementTest::request(qint64 userId) const
{
    return {QStringLiteral("TEST-1"), QStringLiteral("ADMIN_USER_FREEZE"),
            QStringLiteral("S-ADMIN"), {{QStringLiteral("userId"), userId}}};
}

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

QTEST_MAIN(AdminManagementTest)
#include "tst_adminmanagement.moc"
