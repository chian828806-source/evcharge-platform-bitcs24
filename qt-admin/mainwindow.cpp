#include "mainwindow.h"

#include "network/adminsocketclient.h"
#include "shared/protocol/messagetypes.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_client(new AdminSocketClient(this))
{
    setWindowTitle(QStringLiteral("EVCharge 运营管理端"));
    resize(480, 360);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    auto *title = new QLabel(QStringLiteral("管理员登录"), central);
    QFont titleFont = title->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *form = new QFormLayout;
    m_host = new QLineEdit(QStringLiteral("127.0.0.1"), central);
    m_port = new QLineEdit(QStringLiteral("18080"), central);
    m_username = new QLineEdit(QStringLiteral("admin"), central);
    m_password = new QLineEdit(central);
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("服务端"), m_host);
    form->addRow(QStringLiteral("端口"), m_port);
    form->addRow(QStringLiteral("账号"), m_username);
    form->addRow(QStringLiteral("密码"), m_password);
    layout->addLayout(form);
    m_login = new QPushButton(QStringLiteral("登录"), central);
    m_status = new QLabel(QStringLiteral("请输入管理员账号和密码"), central);
    layout->addWidget(m_login);
    layout->addWidget(m_status);
    layout->addStretch();
    setCentralWidget(central);

    connect(m_login, &QPushButton::clicked, this, &MainWindow::submitLogin);
    connect(m_password, &QLineEdit::returnPressed, this, &MainWindow::submitLogin);
    connect(m_client, &AdminSocketClient::connected, this, [this]() {
        m_loginRequestId = m_client->sendRequest(
            MessageTypes::AdminLogin, {}, {
                {QStringLiteral("username"), m_username->text().trimmed()},
                {QStringLiteral("password"), m_password->text()}
            });
        m_status->setText(QStringLiteral("正在验证账号…"));
    });
    connect(m_client, &AdminSocketClient::responseReceived,
            this, &MainWindow::handleResponse);
    connect(m_client, &AdminSocketClient::socketError, this,
            [this](const QString &message) {
                m_login->setEnabled(true);
                m_status->setText(QStringLiteral("连接失败：") + message);
            });
}

void MainWindow::submitLogin()
{
    bool portOk = false;
    const quint16 port = m_port->text().toUShort(&portOk);
    if (!portOk || m_username->text().trimmed().isEmpty() || m_password->text().isEmpty()) {
        m_status->setText(QStringLiteral("请填写有效的服务器、账号和密码"));
        return;
    }
    m_login->setEnabled(false);
    m_status->setText(QStringLiteral("正在连接服务端…"));
    m_client->connectToServer(m_host->text().trimmed(), port);
}

void MainWindow::handleResponse(const QJsonObject &response)
{
    if (response.value(QStringLiteral("requestId")).toString() != m_loginRequestId) {
        return;
    }
    m_login->setEnabled(true);
    if (response.value(QStringLiteral("code")).toInt() != 200) {
        m_status->setText(response.value(QStringLiteral("message")).toString());
        return;
    }
    const QJsonObject data = response.value(QStringLiteral("data")).toObject();
    const QJsonObject admin = data.value(QStringLiteral("admin")).toObject();
    m_status->setText(QStringLiteral("登录成功，欢迎 ")
                      + admin.value(QStringLiteral("displayName")).toString());
}
