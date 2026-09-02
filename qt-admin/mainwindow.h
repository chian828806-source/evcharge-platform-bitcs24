#pragma once

#include <QMainWindow>
#include <QJsonObject>

class AdminSocketClient;
class QLabel;
class QLineEdit;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void submitLogin();
    void handleResponse(const QJsonObject &response);
    void showRevenueSummary(const QJsonObject &data);
    void showRevenueTrend(const QJsonObject &data);
    void showPileStatusSummary(const QJsonObject &data);
    void showPileList(const QJsonObject &data);
    void showStationList(const QJsonObject &data);
    void showUserList(const QJsonObject &data);

    AdminSocketClient *m_client = nullptr;
    QLineEdit *m_host = nullptr;
    QLineEdit *m_port = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QPushButton *m_login = nullptr;
    QLabel *m_status = nullptr;
    QString m_loginRequestId;
    QString m_summaryRequestId;
    QString m_trendRequestId;
    QString m_pileStatusRequestId;
    QString m_pileListRequestId;
    QString m_pileRestartRequestId;
    QString m_stationListRequestId;
    QString m_stationCreateRequestId;
    QString m_userListRequestId;
    QString m_userStatusRequestId;
    QString m_userPhoneKeyword;
    QString m_sessionId;
};
