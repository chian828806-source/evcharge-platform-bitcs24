#include "services/admin/adminmanagementservice.h"
#include "database/databasemanager.h"
#include "shared/protocol/errorcodes.h"

#include <QSqlDatabase>
#include <QSqlQuery>
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
        "CREATE TABLE user(id INTEGER PRIMARY KEY, phone TEXT, status TEXT, updated_at TEXT)")));
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
        "INSERT INTO user VALUES(1, '13800138000', 'NORMAL', '2026-09-02 00:00:00')")));
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

QTEST_MAIN(AdminManagementTest)
#include "tst_adminmanagement.moc"
