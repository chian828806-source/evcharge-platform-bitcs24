#pragma once

#include <QJsonObject>
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
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

    SocketClient *m_socketClient = nullptr;
    QStackedWidget *m_pages = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_orderStatusLabel = nullptr;
    QLabel *m_orderHintLabel = nullptr;
    QLabel *m_balanceLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    QLabel *m_navigationDestination = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_settleButton = nullptr;
    QString m_sessionId;
    QString m_loginRequestId;
    QString m_orderStatus = QStringLiteral("CREATED");
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
    QWidget *buildStationCard(const QString &name, const QString &address,
                              const QString &price, const QString &availability,
                              const QString &distance, bool recommended);
    QWidget *buildPileCard(const QString &number, const QString &type,
                           const QString &power, const QString &status);
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
    void handleResponse(const QJsonObject &response);
};
