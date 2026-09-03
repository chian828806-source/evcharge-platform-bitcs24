#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_server(new QTcpServer(this))
{
    ui->setupUi(this);
    ui->pathEdit->setText(QDir::homePath() + QStringLiteral("/received"));
    connect(ui->listenButton, &QPushButton::clicked, this, &MainWindow::listen);
    connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_transfers);
    delete ui;
}

void MainWindow::listen()
{
    m_dir = ui->pathEdit->text();
    QDir().mkpath(m_dir);
    if (m_server->isListening()) {
        log(QStringLiteral("服务端已经在监听端口 %1").arg(m_server->serverPort()));
        return;
    }
    if (m_server->listen(QHostAddress::Any, quint16(ui->portSpin->value()))) {
        ui->listenerStateLabel->setText(QStringLiteral("● 正在监听 %1").arg(ui->portSpin->value()));
        log(QStringLiteral("监听端口 %1，接收目录 %2").arg(ui->portSpin->value()).arg(m_dir));
    } else {
        ui->listenerStateLabel->setText(QStringLiteral("● 监听失败"));
        log(QStringLiteral("监听失败: ") + m_server->errorString());
    }
}

void MainWindow::newConnection()
{
    while (m_server->hasPendingConnections()) {
        auto *socket = m_server->nextPendingConnection();
        auto *transfer = new FileTransfer(socket, this);
        transfer->setReceiveDirectory(m_dir);
        m_transfers << transfer;
        ui->transferStateLabel->setText(QStringLiteral("客户端已连接，等待文件"));
        log(QStringLiteral("客户端连接: %1").arg(socket->peerAddress().toString()));
        connect(transfer, &FileTransfer::logMessage, this, &MainWindow::log);
        connect(transfer, &FileTransfer::fileReceived, this, [this](const QString &path) {
            ui->transferStateLabel->setText(QStringLiteral("接收完成: %1").arg(QFileInfo(path).fileName()));
        });
        connect(transfer, &FileTransfer::progress, this, [this](qint64 current, qint64 total) {
            ui->progressBar->setMaximum(int(total > 0 ? total : 1));
            ui->progressBar->setValue(int(current));
        });
        connect(transfer, &FileTransfer::failed, this, &MainWindow::log);
    }
}

void MainWindow::log(const QString &message) { ui->logEdit->append(message); }
