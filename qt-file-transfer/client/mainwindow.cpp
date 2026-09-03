#include "mainwindow.h"
#include "dropzone.h"
#include "settingsdialog.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_socket(new QTcpSocket(this)), m_transfer(nullptr)
{
    ui->setupUi(this);
    auto *settings = menuBar()->addAction(QStringLiteral("连接设置"));
    connect(settings, &QAction::triggered, this, [this] {
        SettingsDialog dialog(this);
        dialog.findChild<QLineEdit *>(QStringLiteral("hostEdit"))->setText(ui->hostEdit->text());
        dialog.findChild<QSpinBox *>(QStringLiteral("portSpin"))->setValue(ui->portSpin->value());
        connect(&dialog, &SettingsDialog::settingsReady, this, [this](const QString &host, int port) {
            ui->hostEdit->setText(host);
            ui->portSpin->setValue(port);
        });
        dialog.exec();
    });
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::connectServer);
    connect(ui->chooseButton, &QPushButton::clicked, this, &MainWindow::chooseAndSend);
    connect(ui->dropZone, &DropZone::fileDropped, this, &MainWindow::sendDropped);
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        ui->connectionStateLabel->setText(QStringLiteral("● 已连接"));
        log(QStringLiteral("已连接服务器"));
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        ui->connectionStateLabel->setText(QStringLiteral("● 连接失败"));
        fail(m_socket->errorString());
    });
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::connectServer()
{
    ui->connectionStateLabel->setText(QStringLiteral("● 连接中..."));
    m_socket->connectToHost(ui->hostEdit->text(), quint16(ui->portSpin->value()));
    if (!m_transfer) {
        m_transfer = new FileTransfer(m_socket, this);
        connect(m_transfer, &FileTransfer::logMessage, this, &MainWindow::log);
        connect(m_transfer, &FileTransfer::failed, this, &MainWindow::fail);
        connect(m_transfer, &FileTransfer::progress, this, &MainWindow::updateProgress);
        connect(m_transfer, &FileTransfer::finished, this, [this] {
            ui->transferStateLabel->setText(QStringLiteral("发送完成"));
        });
    }
}

void MainWindow::chooseAndSend()
{
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"));
    if (!file.isEmpty()) sendDropped(file);
}

void MainWindow::sendDropped(const QString &file)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        fail(QStringLiteral("请先连接服务器"));
        return;
    }
    ui->transferStateLabel->setText(QStringLiteral("正在发送: %1").arg(QFileInfo(file).fileName()));
    ui->progressBar->setValue(0);
    m_transfer->sendFile(file);
}

void MainWindow::log(const QString &message) { ui->logEdit->append(message); }

void MainWindow::fail(const QString &message)
{
    log(QStringLiteral("错误: ") + message);
    QMessageBox::warning(this, QStringLiteral("传输失败"), message);
}

void MainWindow::updateProgress(qint64 current, qint64 total)
{
    ui->progressBar->setMaximum(int(total > 0 ? total : 1));
    ui->progressBar->setValue(int(current));
}
