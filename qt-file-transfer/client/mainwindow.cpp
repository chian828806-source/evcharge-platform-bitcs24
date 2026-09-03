#include "mainwindow.h"
#include "dropzone.h"
#include "settingsdialog.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QAction>
MainWindow::MainWindow(QWidget*p):QMainWindow(p),ui(new Ui::MainWindow),m_socket(new QTcpSocket(this)),m_transfer(nullptr){ui->setupUi(this);auto *settings=menuBar()->addAction(QStringLiteral("连接设置"));connect(settings,&QAction::triggered,this,[this]{SettingsDialog dialog(this);dialog.findChild<QLineEdit*>(QStringLiteral("hostEdit"))->setText(ui->hostEdit->text());dialog.findChild<QSpinBox*>(QStringLiteral("portSpin"))->setValue(ui->portSpin->value());connect(&dialog,&SettingsDialog::settingsReady,this,[this](const QString&h,int p){ui->hostEdit->setText(h);ui->portSpin->setValue(p);});dialog.exec();});connect(ui->connectButton,&QPushButton::clicked,this,&MainWindow::connectServer);connect(ui->chooseButton,&QPushButton::clicked,this,&MainWindow::chooseAndSend);connect(ui->dropZone,&DropZone::fileDropped,this,&MainWindow::sendDropped);connect(m_socket,&QTcpSocket::connected,this,[this]{log(QStringLiteral("已连接服务器"));});connect(m_socket,&QTcpSocket::errorOccurred,this,[this](auto){fail(m_socket->errorString());});}
MainWindow::~MainWindow(){delete ui;}
void MainWindow::connectServer(){m_socket->connectToHost(ui->hostEdit->text(),quint16(ui->portSpin->value()));if(!m_transfer){m_transfer=new FileTransfer(m_socket,this);connect(m_transfer,&FileTransfer::logMessage,this,&MainWindow::log);connect(m_transfer,&FileTransfer::failed,this,&MainWindow::fail);connect(m_transfer,&FileTransfer::progress,this,&MainWindow::updateProgress);}}
void MainWindow::chooseAndSend(){const QString f=QFileDialog::getOpenFileName(this,QStringLiteral("选择文件"));if(!f.isEmpty())sendDropped(f);}
void MainWindow::sendDropped(const QString&f){if(m_socket->state()!=QAbstractSocket::ConnectedState){fail(QStringLiteral("请先连接服务器"));return;}m_transfer->sendFile(f);}
void MainWindow::log(const QString&s){ui->logEdit->append(s);}
void MainWindow::fail(const QString&s){log(QStringLiteral("错误: ")+s);QMessageBox::warning(this,QStringLiteral("传输失败"),s);}
void MainWindow::updateProgress(qint64 n,qint64 t){ui->progressBar->setMaximum(int(t>0?t:1));ui->progressBar->setValue(int(n));}
