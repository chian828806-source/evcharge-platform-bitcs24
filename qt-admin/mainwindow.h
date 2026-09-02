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
    void submitLogin();
    void handleResponse(const QJsonObject &response);
    void buildManagementPages();
    void refreshDashboard();
    void requestPileList(const QJsonObject &payload = {});
    void requestStationList();
    void requestUserList();
    QString send(const QString &type, const QJsonObject &payload = {});
    void handleFailure(const QJsonObject &response);
    AdminSocketClient *m_client = nullptr;
    QLineEdit *m_host = nullptr; QLineEdit *m_port = nullptr;
    QLineEdit *m_username = nullptr; QLineEdit *m_password = nullptr;
    QPushButton *m_login = nullptr; QLabel *m_status = nullptr;
    QTabWidget *m_tabs = nullptr; DashboardPage *m_dashboard = nullptr;
    PilePage *m_piles = nullptr; StationPage *m_stations = nullptr;
    UserPage *m_users = nullptr; QTimer *m_dashboardTimer = nullptr;
    QString m_sessionId; QHash<QString, QString> m_requestTypes;
};
