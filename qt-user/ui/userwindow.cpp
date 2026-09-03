#include "userwindow.h"

#include "network/socketclient.h"
#include "shared/protocol/messagetypes.h"

#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

namespace {

QLabel *makeLabel(const QString &text, const char *role = nullptr)
{
    auto *result = new QLabel(text);
    result->setWordWrap(true);
    if (role) {
        result->setProperty("role", role);
    }
    return result;
}

QPushButton *makeButton(const QString &text, const char *kind = "primary")
{
    auto *result = new QPushButton(text);
    result->setCursor(Qt::PointingHandCursor);
    result->setProperty("kind", kind);
    return result;
}

QFrame *makeCard()
{
    auto *result = new QFrame;
    result->setObjectName(QStringLiteral("card"));
    auto *shadow = new QGraphicsDropShadowEffect(result);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(15, 23, 42, 25));
    result->setGraphicsEffect(shadow);
    return result;
}

QScrollArea *makeScrollArea(QWidget *content)
{
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);
    return scrollArea;
}

QString displayMoney(int amountFen)
{
    return QStringLiteral("¥%1").arg(amountFen / 100.0, 0, 'f', 2);
}

} // namespace

UserWindow::UserWindow(QWidget *parent)
    : QMainWindow(parent),
      m_socketClient(new SocketClient(this))
{
    setWindowTitle(QStringLiteral("EVCharge · 充电用户端"));
    setMinimumSize(430, 740);
    resize(460, 820);

    auto *root = new QWidget;
    root->setObjectName(QStringLiteral("appRoot"));
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *connectionBar = new QFrame;
    connectionBar->setObjectName(QStringLiteral("connectionBar"));
    auto *connectionLayout = new QHBoxLayout(connectionBar);
    connectionLayout->setContentsMargins(20, 7, 20, 7);
    m_connectionLabel = makeLabel(QStringLiteral("● 演示模式 · 后端未连接"), "connection");
    connectionLayout->addWidget(m_connectionLabel);
    connectionLayout->addStretch();
    auto *connectButton = makeButton(QStringLiteral("连接本机服务"), "link");
    connectionLayout->addWidget(connectButton);
    rootLayout->addWidget(connectionBar);

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildLoginPage());
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildStationDetailPage());
    m_pages->addWidget(buildChargingPage());
    m_pages->addWidget(buildProfilePage());
    m_pages->addWidget(buildNavigationPage());
    rootLayout->addWidget(m_pages, 1);
    setCentralWidget(root);

    connect(connectButton, &QPushButton::clicked, this, [this]() {
        if (m_socketClient->isConnected()) {
            m_socketClient->disconnectFromServer();
        } else {
            m_connectionLabel->setText(QStringLiteral("● 正在连接 127.0.0.1:18080…"));
            m_socketClient->connectToServer(QStringLiteral("127.0.0.1"), 18080);
        }
    });
    connect(m_socketClient, &SocketClient::connected, this,
            [this]() { setConnected(true); });
    connect(m_socketClient, &SocketClient::disconnected, this,
            [this]() { setConnected(false); });
    connect(m_socketClient, &SocketClient::socketError, this,
            [this](const QString &message) {
                setConnected(false);
                showNotice(QStringLiteral("连接失败：%1；仍可预览演示界面").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::requestTimedOut, this,
            [this](const QString &requestId, const QString &) {
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                showNotice(QStringLiteral("请求超时，请检查服务后重试"), true);
            });
    connect(m_socketClient, &SocketClient::requestFailed, this,
            [this](const QString &requestId, const QString &, const QString &message) {
                if (requestId == m_loginRequestId) {
                    m_loginRequestId.clear();
                }
                showNotice(QStringLiteral("请求失败：%1").arg(message), true);
            });
    connect(m_socketClient, &SocketClient::responseReceived,
            this, &UserWindow::handleResponse);
}

QWidget *UserWindow::buildPageHeader(const QString &eyebrow,
                                     const QString &title,
                                     const QString &subtitle)
{
    auto *header = new QWidget;
    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(4, 20, 4, 12);
    layout->setSpacing(5);
    layout->addWidget(makeLabel(eyebrow.toUpper(), "eyebrow"));
    layout->addWidget(makeLabel(title, "pageTitle"));
    if (!subtitle.isEmpty()) {
        layout->addWidget(makeLabel(subtitle, "subtitle"));
    }
    return header;
}

QWidget *UserWindow::buildLoginPage()
{
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("loginPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(34, 48, 34, 42);
    layout->setSpacing(17);
    layout->addStretch();

    auto *mark = makeLabel(QStringLiteral("⚡"), "brandMark");
    mark->setAlignment(Qt::AlignCenter);
    layout->addWidget(mark, 0, Qt::AlignCenter);
    auto *title = makeLabel(QStringLiteral("一路满电，随时出发"), "heroTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *subtitle = makeLabel(QStringLiteral("EVCharge 智慧充电用户端"), "subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);
    layout->addSpacing(15);

    auto *loginCard = makeCard();
    auto *form = new QVBoxLayout(loginCard);
    form->setContentsMargins(24, 24, 24, 24);
    form->setSpacing(12);
    form->addWidget(makeLabel(QStringLiteral("手机号登录"), "sectionTitle"));
    form->addWidget(makeLabel(QStringLiteral("新手机号将自动完成注册，无需密码"), "caption"));
    m_phoneEdit = new QLineEdit;
    m_phoneEdit->setPlaceholderText(QStringLiteral("请输入 11 位手机号"));
    m_phoneEdit->setMaxLength(11);
    m_phoneEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[0-9]{0,11}")), m_phoneEdit));
    form->addWidget(m_phoneEdit);
    auto *loginButton = makeButton(QStringLiteral("登录 / 自动注册"));
    loginButton->setMinimumHeight(48);
    form->addWidget(loginButton);
    form->addWidget(makeLabel(
        QStringLiteral("未连接后端时，输入合法手机号即可使用 Mock 数据预览 UI。"), "hint"));
    layout->addWidget(loginCard);
    layout->addStretch(2);

    connect(loginButton, &QPushButton::clicked, this, &UserWindow::attemptLogin);
    connect(m_phoneEdit, &QLineEdit::returnPressed, this, &UserWindow::attemptLogin);
    return page;
}

QWidget *UserWindow::buildHomePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    layout->addWidget(buildPageHeader(QStringLiteral("EVCharge"),
                                      QStringLiteral("附近充电站"),
                                      QStringLiteral("下午好，用户8000 · 当前位置：甘井子区")));

    auto *locationCard = makeCard();
    auto *locationLayout = new QVBoxLayout(locationCard);
    locationLayout->setContentsMargins(18, 18, 18, 18);
    locationLayout->setSpacing(10);
    locationLayout->addWidget(makeLabel(QStringLiteral("模拟当前位置"), "sectionTitle"));
    auto *locationRow = new QHBoxLayout;
    auto *districtBox = new QComboBox;
    districtBox->addItems({QStringLiteral("甘井子区"), QStringLiteral("高新园区"),
                           QStringLiteral("沙河口区"), QStringLiteral("中山区")});
    auto *addressEdit = new QLineEdit(QStringLiteral("软件园路"));
    addressEdit->setPlaceholderText(QStringLiteral("输入街道或地标"));
    auto *locateButton = makeButton(QStringLiteral("定位"), "secondary");
    locationRow->addWidget(districtBox);
    locationRow->addWidget(addressEdit, 1);
    locationRow->addWidget(locateButton);
    locationLayout->addLayout(locationRow);
    layout->addWidget(locationCard);

    auto *headingRow = new QHBoxLayout;
    headingRow->addWidget(makeLabel(QStringLiteral("为你推荐"), "sectionTitle"));
    headingRow->addStretch();
    headingRow->addWidget(makeLabel(QStringLiteral("按距离从近到远"), "caption"));
    layout->addLayout(headingRow);
    layout->addWidget(buildStationCard(QStringLiteral("软件园智慧充电站"),
                                       QStringLiteral("软件园路 8 号"),
                                       QStringLiteral("综合价 ¥1.10 / 度"),
                                       QStringLiteral("2 / 4 空闲"),
                                       QStringLiteral("1.25 km"), true));
    layout->addWidget(buildStationCard(QStringLiteral("万达广场充电中心"),
                                       QStringLiteral("虹韵路 6 号 B2 层"),
                                       QStringLiteral("综合价 ¥1.18 / 度"),
                                       QStringLiteral("5 / 12 空闲"),
                                       QStringLiteral("2.80 km"), false));
    layout->addWidget(buildStationCard(QStringLiteral("星海绿色能源站"),
                                       QStringLiteral("中山路 608 号"),
                                       QStringLiteral("综合价 ¥0.98 / 度"),
                                       QStringLiteral("1 / 8 空闲"),
                                       QStringLiteral("4.36 km"), false));
    layout->addStretch();

    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Home));
    connect(locateButton, &QPushButton::clicked, this,
            [this]() { showNotice(QStringLiteral("已按模拟位置刷新附近站点")); });
    return page;
}

QWidget *UserWindow::buildStationCard(const QString &name, const QString &address,
                                      const QString &price, const QString &availability,
                                      const QString &distance, bool recommended)
{
    auto *stationCard = makeCard();
    auto *layout = new QVBoxLayout(stationCard);
    layout->setContentsMargins(18, 17, 18, 17);
    layout->setSpacing(10);
    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(makeLabel(name, "cardTitle"));
    titleRow->addStretch();
    if (recommended) {
        titleRow->addWidget(makeLabel(QStringLiteral("低拥堵推荐"), "badgeGood"));
    }
    layout->addLayout(titleRow);
    layout->addWidget(makeLabel(QStringLiteral("⌖ %1").arg(address), "caption"));
    auto *metrics = new QHBoxLayout;
    metrics->addWidget(makeLabel(price, "metric"));
    metrics->addWidget(makeLabel(availability, "metric"));
    metrics->addStretch();
    metrics->addWidget(makeLabel(distance, "distance"));
    layout->addLayout(metrics);
    auto *actions = new QHBoxLayout;
    auto *detailButton = makeButton(QStringLiteral("查看详情"), "secondary");
    auto *navigationButton = makeButton(QStringLiteral("路线导航"), "ghost");
    actions->addWidget(detailButton, 1);
    actions->addWidget(navigationButton, 1);
    layout->addLayout(actions);
    connect(detailButton, &QPushButton::clicked, this,
            [this]() { showPage(StationDetail); });
    connect(navigationButton, &QPushButton::clicked, this, [this, name]() {
        m_navigationDestination->setText(name);
        showPage(Navigation);
    });
    return stationCard;
}

QWidget *UserWindow::buildStationDetailPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(13);
    layout->addWidget(buildPageHeader(QStringLiteral("STATION DETAIL"),
                                      QStringLiteral("软件园智慧充电站"),
                                      QStringLiteral("软件园路 8 号 · 正常营业")));

    auto *summaryCard = makeCard();
    auto *summaryLayout = new QHBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(18, 16, 18, 16);
    summaryLayout->addWidget(makeLabel(QStringLiteral("¥0.80 + ¥0.30\n电费 + 服务费"), "metricLarge"));
    summaryLayout->addStretch();
    summaryLayout->addWidget(makeLabel(QStringLiteral("2 / 4\n当前空闲"), "metricLarge"));
    summaryLayout->addStretch();
    auto *navigationButton = makeButton(QStringLiteral("导航"), "secondary");
    summaryLayout->addWidget(navigationButton);
    layout->addWidget(summaryCard);
    layout->addWidget(makeLabel(QStringLiteral("选择充电桩"), "sectionTitle"));
    layout->addWidget(buildPileCard(QStringLiteral("P01"), QStringLiteral("快充"),
                                    QStringLiteral("60 kW"), QStringLiteral("AVAILABLE")));
    layout->addWidget(buildPileCard(QStringLiteral("P02"), QStringLiteral("快充"),
                                    QStringLiteral("60 kW"), QStringLiteral("CHARGING")));
    layout->addWidget(buildPileCard(QStringLiteral("P03"), QStringLiteral("慢充"),
                                    QStringLiteral("7 kW"), QStringLiteral("AVAILABLE")));
    layout->addWidget(buildPileCard(QStringLiteral("P04"), QStringLiteral("慢充"),
                                    QStringLiteral("7 kW"), QStringLiteral("FAULT")));
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Home));
    connect(navigationButton, &QPushButton::clicked, this, [this]() {
        m_navigationDestination->setText(QStringLiteral("软件园智慧充电站"));
        showPage(Navigation);
    });
    return page;
}

QWidget *UserWindow::buildPileCard(const QString &number, const QString &type,
                                   const QString &power, const QString &status)
{
    auto *pileCard = makeCard();
    auto *layout = new QHBoxLayout(pileCard);
    layout->setContentsMargins(17, 15, 17, 15);
    auto *information = new QVBoxLayout;
    information->addWidget(makeLabel(number + QStringLiteral(" · ") + type, "cardTitle"));
    information->addWidget(makeLabel(power + QStringLiteral(" · ") + status, "caption"));
    layout->addLayout(information);
    layout->addStretch();
    const bool available = status == QStringLiteral("AVAILABLE");
    auto *selectButton = makeButton(available ? QStringLiteral("选择") : QStringLiteral("不可用"),
                                    available ? "primary" : "disabled");
    selectButton->setEnabled(available);
    layout->addWidget(selectButton);
    connect(selectButton, &QPushButton::clicked, this, [this, number]() {
        const auto choice = QMessageBox::question(
            this, QStringLiteral("创建预约"),
            QStringLiteral("确认选择充电桩 %1？服务端将创建待开始订单。").arg(number));
        if (choice == QMessageBox::Yes) {
            if (!isDemoMode()) {
                showNotice(QStringLiteral("真实订单创建待接入 ORDER_CREATE；未修改本地订单状态"), true);
                return;
            }
            setOrderStatus(QStringLiteral("CREATED"));
            showPage(Charging);
            showNotice(QStringLiteral("Mock 订单已创建"));
        }
    });
    return pileCard;
}

QWidget *UserWindow::buildChargingPage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    layout->addWidget(buildPageHeader(QStringLiteral("ACTIVE ORDER"),
                                      QStringLiteral("当前充电"),
                                      QStringLiteral("时长、电量和金额均以服务端数据为准")));

    auto *orderCard = makeCard();
    auto *orderLayout = new QVBoxLayout(orderCard);
    orderLayout->setContentsMargins(20, 19, 20, 19);
    auto *orderTop = new QHBoxLayout;
    orderTop->addWidget(makeLabel(QStringLiteral("软件园智慧充电站"), "cardTitle"));
    orderTop->addStretch();
    m_orderStatusLabel = makeLabel(QStringLiteral("待开始"), "badgeWarn");
    orderTop->addWidget(m_orderStatusLabel);
    orderLayout->addLayout(orderTop);
    orderLayout->addWidget(makeLabel(
        QStringLiteral("P01 · 快充 60 kW · O202609020001"), "caption"));
    layout->addWidget(orderCard);

    auto *progressCard = makeCard();
    auto *progressLayout = new QVBoxLayout(progressCard);
    progressLayout->setContentsMargins(20, 20, 20, 20);
    progressLayout->setSpacing(15);
    progressLayout->addWidget(makeLabel(QStringLiteral("本次充电"), "sectionTitle"));
    auto *statistics = new QHBoxLayout;
    statistics->addWidget(makeLabel(QStringLiteral("05:18\n充电时长"), "metricLarge"));
    statistics->addWidget(makeLabel(QStringLiteral("5.00 kWh\n已充电量"), "metricLarge"));
    statistics->addWidget(makeLabel(QStringLiteral("¥5.50\n预估金额"), "metricLarge"));
    progressLayout->addLayout(statistics);
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setValue(38);
    progress->setTextVisible(false);
    progressLayout->addWidget(progress);
    m_orderHintLabel = makeLabel(QString(), "hint");
    progressLayout->addWidget(m_orderHintLabel);
    layout->addWidget(progressCard);

    auto *actions = new QHBoxLayout;
    m_startButton = makeButton(QStringLiteral("开始充电"));
    m_cancelButton = makeButton(QStringLiteral("取消预约"), "dangerGhost");
    m_stopButton = makeButton(QStringLiteral("停止充电"), "danger");
    m_settleButton = makeButton(QStringLiteral("确认结算"));
    actions->addWidget(m_startButton);
    actions->addWidget(m_cancelButton);
    actions->addWidget(m_stopButton);
    actions->addWidget(m_settleButton);
    layout->addLayout(actions);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Charging));

    connect(m_startButton, &QPushButton::clicked, this,
            [this]() {
                if (!isDemoMode()) {
                    showNotice(QStringLiteral("真实启动待接入 ORDER_START；未修改本地订单状态"), true);
                    return;
                }
                setOrderStatus(QStringLiteral("CHARGING"));
                showNotice(QStringLiteral("Mock 充电已开始"));
            });
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("取消预约"),
                                  QStringLiteral("确认取消尚未开始的订单？")) == QMessageBox::Yes) {
            if (!isDemoMode()) {
                showNotice(QStringLiteral("真实取消待接入 ORDER_CANCEL；未修改本地订单状态"), true);
                return;
            }
            setOrderStatus(QStringLiteral("CANCELLED"));
            showNotice(QStringLiteral("Mock 订单已取消"));
        }
    });
    connect(m_stopButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("停止充电"),
                                  QStringLiteral("确认停止充电并生成待结算金额？")) == QMessageBox::Yes) {
            if (!isDemoMode()) {
                showNotice(QStringLiteral("真实停止待接入 ORDER_STOP；未修改本地订单状态"), true);
                return;
            }
            setOrderStatus(QStringLiteral("PENDING_PAYMENT"));
            showNotice(QStringLiteral("Mock 充电已停止"));
        }
    });
    connect(m_settleButton, &QPushButton::clicked, this, [this]() {
        if (!isDemoMode()) {
            showNotice(QStringLiteral("真实结算待接入 ORDER_SETTLE；未修改本地订单状态"), true);
            return;
        }
        setOrderStatus(QStringLiteral("COMPLETED"));
        showNotice(QStringLiteral("Mock 结算完成，欢迎下次使用"));
    });
    setOrderStatus(m_orderStatus);
    return page;
}

QWidget *UserWindow::buildProfilePage()
{
    auto *page = new QWidget;
    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(20, 0, 20, 24);
    layout->setSpacing(14);
    layout->addWidget(buildPageHeader(QStringLiteral("MY EVCHARGE"),
                                      QStringLiteral("我的"),
                                      QStringLiteral("账户、钱包与充电记录")));

    auto *profileCard = makeCard();
    auto *profileLayout = new QHBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 20, 20, 20);
    auto *avatarLabel = makeLabel(QStringLiteral("U"), "avatar");
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setFixedSize(58, 58);
    profileLayout->addWidget(avatarLabel);
    auto *identity = new QVBoxLayout;
    m_nicknameLabel = makeLabel(QStringLiteral("用户8000"), "cardTitle");
    identity->addWidget(m_nicknameLabel);
    identity->addWidget(makeLabel(QStringLiteral("138****8000 · 普通用户"), "caption"));
    profileLayout->addLayout(identity, 1);
    auto *avatarButton = makeButton(QStringLiteral("头像"), "ghost");
    auto *renameButton = makeButton(QStringLiteral("昵称"), "ghost");
    profileLayout->addWidget(avatarButton);
    profileLayout->addWidget(renameButton);
    layout->addWidget(profileCard);

    auto *walletCard = makeCard();
    auto *walletLayout = new QHBoxLayout(walletCard);
    walletLayout->setContentsMargins(20, 19, 20, 19);
    auto *walletInformation = new QVBoxLayout;
    walletInformation->addWidget(makeLabel(QStringLiteral("钱包余额"), "caption"));
    m_balanceLabel = makeLabel(displayMoney(m_balanceFenInFen), "walletAmount");
    walletInformation->addWidget(m_balanceLabel);
    walletLayout->addLayout(walletInformation);
    walletLayout->addStretch();
    auto *rechargeButton = makeButton(QStringLiteral("充值"));
    walletLayout->addWidget(rechargeButton);
    layout->addWidget(walletCard);

    layout->addWidget(makeLabel(QStringLiteral("最近订单"), "sectionTitle"));
    layout->addWidget(buildOrderCard(QStringLiteral("软件园智慧充电站"),
                                     QStringLiteral("今天 16:00 · P01 · 5.00 kWh"),
                                     QStringLiteral("¥5.50"), QStringLiteral("待结算")));
    layout->addWidget(buildOrderCard(QStringLiteral("万达广场充电中心"),
                                     QStringLiteral("08-30 12:24 · A07 · 18.60 kWh"),
                                     QStringLiteral("¥21.95"), QStringLiteral("已完成")));
    auto *logoutButton = makeButton(QStringLiteral("退出登录"), "dangerGhost");
    layout->addWidget(logoutButton);
    layout->addStretch();
    pageLayout->addWidget(makeScrollArea(content), 1);
    pageLayout->addWidget(buildBottomNavigation(Profile));

    connect(renameButton, &QPushButton::clicked, this, &UserWindow::showRenameDialog);
    connect(rechargeButton, &QPushButton::clicked, this, &UserWindow::showRechargeDialog);
    connect(avatarButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择头像"), {}, QStringLiteral("图片 (*.png *.jpg *.jpeg)"));
        if (!path.isEmpty()) {
            showNotice(isDemoMode()
                ? QStringLiteral("已选择头像（Mock 预览，未上传）")
                : QStringLiteral("真实头像上传待接入 USER_AVATAR_UPLOAD；文件未上传"));
        }
    });
    connect(logoutButton, &QPushButton::clicked, this, [this]() {
        m_sessionId.clear();
        showPage(Login);
    });
    return page;
}

QWidget *UserWindow::buildOrderCard(const QString &station, const QString &description,
                                    const QString &amount, const QString &status)
{
    auto *orderCard = makeCard();
    auto *layout = new QHBoxLayout(orderCard);
    layout->setContentsMargins(17, 15, 17, 15);
    auto *information = new QVBoxLayout;
    information->addWidget(makeLabel(station, "cardTitle"));
    information->addWidget(makeLabel(description, "caption"));
    layout->addLayout(information, 1);
    auto *summary = new QVBoxLayout;
    auto *amountLabel = makeLabel(amount, "metric");
    amountLabel->setAlignment(Qt::AlignRight);
    summary->addWidget(amountLabel);
    auto *statusLabel = makeLabel(status, status == QStringLiteral("待结算")
                                              ? "badgeWarn" : "badgeNeutral");
    statusLabel->setAlignment(Qt::AlignCenter);
    summary->addWidget(statusLabel);
    layout->addLayout(summary);
    return orderCard;
}

QWidget *UserWindow::buildNavigationPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 18, 20, 24);
    layout->setSpacing(14);
    auto *backButton = makeButton(QStringLiteral("← 返回"), "ghost");
    backButton->setMaximumWidth(90);
    layout->addWidget(backButton);
    layout->addWidget(makeLabel(QStringLiteral("路线导航"), "pageTitle"));

    auto *routeCard = makeCard();
    auto *routeLayout = new QVBoxLayout(routeCard);
    routeLayout->setContentsMargins(20, 18, 20, 18);
    routeLayout->addWidget(makeLabel(QStringLiteral("起点 · 软件园路"), "caption"));
    m_navigationDestination = makeLabel(QStringLiteral("软件园智慧充电站"), "cardTitle");
    routeLayout->addWidget(m_navigationDestination);
    auto *modeRow = new QHBoxLayout;
    auto *driveButton = makeButton(QStringLiteral("驾车 · 8 分钟"));
    auto *walkButton = makeButton(QStringLiteral("步行 · 22 分钟"), "secondary");
    modeRow->addWidget(driveButton);
    modeRow->addWidget(walkButton);
    routeLayout->addLayout(modeRow);
    layout->addWidget(routeCard);

    auto *mapPlaceholder = new QFrame;
    mapPlaceholder->setObjectName(QStringLiteral("mapPlaceholder"));
    auto *mapLayout = new QVBoxLayout(mapPlaceholder);
    mapLayout->setAlignment(Qt::AlignCenter);
    auto *pin = makeLabel(QStringLiteral("⌖"), "mapPin");
    pin->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(pin, 0, Qt::AlignCenter);
    auto *mapTitle = makeLabel(QStringLiteral("地图容器占位"), "sectionTitle");
    mapTitle->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(mapTitle);
    auto *mapHint = makeLabel(
        QStringLiteral("导航页面 UI 已完成；QWebEngineView + 腾讯地图 Web API\n待后续联调实现。"), "subtitle");
    mapHint->setAlignment(Qt::AlignCenter);
    mapLayout->addWidget(mapHint);
    layout->addWidget(mapPlaceholder, 1);

    connect(backButton, &QPushButton::clicked, this,
            [this]() { showPage(Home); });
    connect(driveButton, &QPushButton::clicked, this,
            [this]() { showNotice(QStringLiteral("已选择驾车路线")); });
    connect(walkButton, &QPushButton::clicked, this,
            [this]() { showNotice(QStringLiteral("已选择步行路线")); });
    return page;
}

QWidget *UserWindow::buildBottomNavigation(Page activePage)
{
    auto *navigation = new QFrame;
    navigation->setObjectName(QStringLiteral("bottomNavigation"));
    auto *layout = new QHBoxLayout(navigation);
    layout->setContentsMargins(12, 8, 12, 10);
    layout->setSpacing(8);
    const QList<QPair<QString, Page>> items{
        {QStringLiteral("⌂\n首页"), Home},
        {QStringLiteral("ϟ\n充电"), Charging},
        {QStringLiteral("●\n我的"), Profile}
    };
    for (const auto &item : items) {
        auto *navigationButton = makeButton(
            item.first, item.second == activePage ? "navActive" : "nav");
        navigationButton->setMinimumHeight(52);
        layout->addWidget(navigationButton, 1);
        connect(navigationButton, &QPushButton::clicked, this,
                [this, page = item.second]() { showPage(page); });
    }
    return navigation;
}

void UserWindow::showPage(Page page)
{
    m_pages->setCurrentIndex(static_cast<int>(page));
}

void UserWindow::attemptLogin()
{
    const QString phone = m_phoneEdit->text().trimmed();
    if (phone.size() != 11 || !phone.startsWith(QLatin1Char('1'))) {
        showNotice(QStringLiteral("请输入正确的 11 位手机号"), true);
        return;
    }
    if (m_socketClient->isConnected()) {
        m_loginRequestId = m_socketClient->sendRequest(
            MessageTypes::UserLogin, {},
            QJsonObject{{QStringLiteral("phone"), phone}});
        showNotice(m_loginRequestId.isEmpty() ? QStringLiteral("登录请求发送失败")
                                              : QStringLiteral("正在登录…"),
                   m_loginRequestId.isEmpty());
        return;
    }
    m_sessionId = QStringLiteral("DEMO-SESSION");
    showPage(Home);
    showNotice(QStringLiteral("已进入演示模式；接入后端后使用真实数据"));
}

bool UserWindow::isDemoMode() const
{
    return !m_socketClient->isConnected();
}

void UserWindow::setConnected(bool connected)
{
    m_connectionLabel->setText(connected
        ? QStringLiteral("● 服务已连接 · 127.0.0.1:18080")
        : QStringLiteral("● 演示模式 · 后端未连接"));
    m_connectionLabel->setProperty("online", connected);
    m_connectionLabel->style()->unpolish(m_connectionLabel);
    m_connectionLabel->style()->polish(m_connectionLabel);
}

void UserWindow::setOrderStatus(const QString &status)
{
    m_orderStatus = status;
    if (!m_orderStatusLabel) {
        return;
    }
    const bool created = status == QStringLiteral("CREATED");
    const bool charging = status == QStringLiteral("CHARGING");
    const bool pendingPayment = status == QStringLiteral("PENDING_PAYMENT");
    const bool completed = status == QStringLiteral("COMPLETED");
    m_startButton->setVisible(created);
    m_cancelButton->setVisible(created);
    m_stopButton->setVisible(charging);
    m_settleButton->setVisible(pendingPayment);
    m_orderStatusLabel->setText(created ? QStringLiteral("待开始")
        : charging ? QStringLiteral("充电中")
        : pendingPayment ? QStringLiteral("待结算")
        : completed ? QStringLiteral("已完成") : QStringLiteral("已取消"));
    m_orderHintLabel->setText(created
        ? QStringLiteral("订单已创建，请确认车辆连接后开始充电。")
        : charging ? QStringLiteral("页面将定时获取服务端返回的权威充电进度。")
        : pendingPayment ? QStringLiteral("充电已停止，请核对应付金额并完成钱包结算。")
        : completed ? QStringLiteral("订单已完成，电桩已经释放。")
                    : QStringLiteral("预约已取消，电桩已经释放。"));
}

void UserWindow::showRechargeDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("钱包充值"));
    auto *layout = new QFormLayout(&dialog);
    auto *amountEdit = new QLineEdit;
    amountEdit->setPlaceholderText(QStringLiteral("例如：50.00"));
    layout->addRow(QStringLiteral("充值金额（元）"), amountEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    bool valid = false;
    const double amountYuan = amountEdit->text().toDouble(&valid);
    if (!valid || amountYuan <= 0.0) {
        showNotice(QStringLiteral("请输入有效的充值金额"), true);
        return;
    }
    if (!isDemoMode()) {
        showNotice(QStringLiteral("真实充值待接入 USER_RECHARGE；余额未改变"), true);
        return;
    }
    m_balanceFenInFen += qRound(amountYuan * 100.0);
    m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
    showNotice(QStringLiteral("Mock 充值成功；真实模式将等待服务端确认"));
}

void UserWindow::showRenameDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("修改昵称"));
    auto *layout = new QFormLayout(&dialog);
    auto *nicknameEdit = new QLineEdit(m_nicknameLabel->text());
    nicknameEdit->setMaxLength(20);
    layout->addRow(QStringLiteral("昵称"), nicknameEdit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString nickname = nicknameEdit->text().trimmed();
    if (nickname.size() < 2) {
        showNotice(QStringLiteral("昵称长度应为 2–20 个字符"), true);
        return;
    }
    if (!isDemoMode()) {
        showNotice(QStringLiteral("真实资料更新待接入 USER_PROFILE_UPDATE；昵称未改变"), true);
        return;
    }
    m_nicknameLabel->setText(nickname);
    showNotice(QStringLiteral("昵称已更新（Mock 数据）"));
}

void UserWindow::showNotice(const QString &message, bool error)
{
    auto *notice = new QLabel(message, centralWidget());
    notice->setObjectName(error ? QStringLiteral("errorNotice")
                                : QStringLiteral("successNotice"));
    notice->setWordWrap(true);
    notice->setAlignment(Qt::AlignCenter);
    notice->setGeometry(28, centralWidget()->height() - 120,
                        centralWidget()->width() - 56, 54);
    notice->raise();
    notice->show();
    QTimer::singleShot(2800, notice, &QLabel::deleteLater);
}

void UserWindow::handleResponse(const QJsonObject &response)
{
    const QString requestId = response.value(QStringLiteral("requestId")).toString();
    const int code = response.value(QStringLiteral("code")).toInt();
    if (code == 4003) {
        m_sessionId.clear();
        showPage(Login);
        showNotice(QStringLiteral("会话已失效，请重新登录"), true);
        return;
    }
    if (code != 200) {
        if (requestId == m_loginRequestId) {
            m_loginRequestId.clear();
        }
        showNotice(response.value(QStringLiteral("message")).toString(), true);
        return;
    }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    if (requestId == m_loginRequestId
        && data.value(QStringLiteral("sessionId")).isString()) {
        m_sessionId = data.value(QStringLiteral("sessionId")).toString();
        const QJsonObject user = data.value(QStringLiteral("user")).toObject();
        if (user.value(QStringLiteral("nickname")).isString() && m_nicknameLabel) {
            m_nicknameLabel->setText(user.value(QStringLiteral("nickname")).toString());
        }
        if (user.value(QStringLiteral("balanceFen")).isDouble() && m_balanceLabel) {
            m_balanceFenInFen = user.value(QStringLiteral("balanceFen")).toInt();
            m_balanceLabel->setText(displayMoney(m_balanceFenInFen));
        }
        m_loginRequestId.clear();
        showPage(Home);
        showNotice(QStringLiteral("登录成功"));
    }
}
