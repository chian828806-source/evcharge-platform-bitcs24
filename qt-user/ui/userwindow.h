#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class QTimer;
class SocketClient;

class UserWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit UserWindow(QWidget *parent = nullptr);

private:
    enum Page {
        Login,
        Home,
        StationDetail,
        Charging,
        Profile,
        Navigation
    };

    enum class SessionMode {
        None,
        Demo,
        Real
    };

    SocketClient *m_socketClient = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_orderStatusLabel = nullptr;
    QLabel *m_orderHintLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_navigationDestination = nullptr;
    QLabel *m_profilePhoneLabel = nullptr;
    QLabel *m_orderSummaryLabel = nullptr;
    QLabel *m_chargeStatisticsLabel = nullptr;
    QLabel *m_stationDetailTitle = nullptr;
    QLabel *m_stationDetailSummary = nullptr;
    QLabel *m_avatarLabel = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_settleButton = nullptr;
    QString m_sessionId;
    SessionMode m_sessionMode = SessionMode::None;
    QString m_loginRequestId;
    QHash<QString, QString> m_requestTypes;
    QString m_orderStatus = QStringLiteral("CREATED");
    QJsonObject m_activeOrder;
    QJsonObject m_selectedStation;
    QJsonArray m_nearbyStations;
    QJsonArray m_recommendedStations;
    QVBoxLayout *m_stationListLayout = nullptr;
    QVBoxLayout *m_pileListLayout = nullptr;
    QVBoxLayout *m_orderListLayout = nullptr;
    QTimer *m_orderPollTimer = nullptr;
    int m_balanceFenInFen = 12860;

    QWidget *buildLoginPage();
    QWidget *buildHomePage();
    QWidget *buildStationDetailPage();
    QWidget *buildChargingPage();
    QWidget *buildProfilePage();
    QWidget *buildNavigationPage();
    QWidget *buildPageHeader(const QString &eyebrow, const QString &title,
                             const QString &subtitle = {});
    QWidget *buildBottomNavigation(Page activePage);
    QWidget *buildStationCard(const QJsonObject &station);
    QWidget *buildPileCard(const QJsonObject &pile);
    QWidget *buildOrderCard(const QString &station, const QString &description,
                            const QString &amount, const QString &status);

    void showPage(Page page);
    void attemptLogin();
    bool isDemoMode() const;
    void setConnected(bool connected);
    void setOrderStatus(const QString &status);
    void showNotice(const QString &message, bool error = false);
    void showRechargeDialog();
    void showRenameDialog();
    void uploadAvatar();
    QString sendRequest(const QString &type, const QJsonObject &payload = {});
    void requestInitialData();
    void loadDemoData();
    void requestActiveOrder();
    void applyUser(const QJsonObject &user);
    void applyOrder(const QJsonObject &order);
    void renderStations(const QJsonArray &stations);
    void renderStationDetail(const QJsonObject &station, const QJsonArray &piles);
    void renderOrders(const QJsonArray &orders);
    void clearLayout(QVBoxLayout *layout);
    void handleResponse(const QJsonObject &response);
};
