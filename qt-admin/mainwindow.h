#pragma once
#include <QHash>
#include <QJsonObject>
#include <QMainWindow>
class AdminSocketClient; class DashboardPage; class QLabel; class QLineEdit;
class PilePage; class QPushButton; class QTabWidget; class QTimer;
class StationPage; class UserPage;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public: explicit MainWindow(QWidget *parent = nullptr);
private:
    void buildLoginPage();
    void submitLogin();
    void handleResponse(const QJsonObject &response);
    void buildManagementPages();
    void refreshDashboard();
    void requestPileList(const QJsonObject &payload = {});
    void requestStationPileDetails(qint64 stationId);
    void requestStationList();
    void requestUserList();
    QString send(const QString &type, const QJsonObject &payload = {});
    void handleFailure(const QJsonObject &response, const QString &type);
    void finishAction(const QString &type);
    AdminSocketClient *m_client = nullptr;
    QLineEdit *m_host = nullptr; QLineEdit *m_port = nullptr;
    QLineEdit *m_username = nullptr; QLineEdit *m_password = nullptr;
    QPushButton *m_login = nullptr; QLabel *m_status = nullptr;
    QTabWidget *m_tabs = nullptr; DashboardPage *m_dashboard = nullptr;
    PilePage *m_piles = nullptr; StationPage *m_stations = nullptr;
    UserPage *m_users = nullptr; QTimer *m_dashboardTimer = nullptr;
    QString m_sessionId; QHash<QString, QString> m_requestTypes;
    QHash<QString, qint64> m_stationDetailRequests;
    int m_trendDays = 7;
    QString m_warningHorizon = QStringLiteral("1h");
    QString m_lastHost = QStringLiteral("127.0.0.1");
    quint16 m_lastPort = 18080;
};
