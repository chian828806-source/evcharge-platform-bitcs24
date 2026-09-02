#include "services/admin/adminmanagementservice.h"
#include "shared/protocol/errorcodes.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest>

class AdminManagementTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void freezeAndUnfreezeAreIdempotent();
    void missingUserReturnsDocumentedError();

private:
    RequestMessage request(qint64 userId) const;
    QSqlDatabase m_database;
    AdminManagementService *m_service = nullptr;
};

void AdminManagementTest::initTestCase()
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                            QStringLiteral("admin-test"));
    m_database.setDatabaseName(QStringLiteral(":memory:"));
    QVERIFY(m_database.open());
    QSqlQuery query(m_database);
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE user(id INTEGER PRIMARY KEY, phone TEXT, status TEXT, updated_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "CREATE TABLE operation_log(id INTEGER PRIMARY KEY AUTOINCREMENT, admin_id INTEGER, "
        "action TEXT, target_type TEXT, target_id INTEGER, before_status TEXT, "
        "after_status TEXT, result TEXT, message TEXT, created_at TEXT)")));
    QVERIFY(query.exec(QStringLiteral(
        "INSERT INTO user VALUES(1, '13800138000', 'NORMAL', '2026-09-02 00:00:00')")));
    m_service = new AdminManagementService(m_database, this);
}

void AdminManagementTest::cleanupTestCase()
{
    delete m_service;
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(QStringLiteral("admin-test"));
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

QTEST_MAIN(AdminManagementTest)
#include "tst_adminmanagement.moc"
