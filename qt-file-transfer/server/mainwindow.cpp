#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
MainWindow::MainWindow(QWidget*p):QMainWindow(p),ui(new Ui::MainWindow),m_server(new QTcpServer(this)){ui->setupUi(this);ui->pathEdit->setText(QDir::homePath()+QStringLiteral("/received"));connect(ui->listenButton,&QPushButton::clicked,this,&MainWindow::listen);connect(m_server,&QTcpServer::newConnection,this,&MainWindow::newConnection);}
MainWindow::~MainWindow(){qDeleteAll(m_transfers);delete ui;}
void MainWindow::listen(){m_dir=ui->pathEdit->text();QDir().mkpath(m_dir);if(m_server->listen(QHostAddress::Any,quint16(ui->portSpin->value())))log(QStringLiteral("监听端口 %1，接收目录 %2").arg(ui->portSpin->value()).arg(m_dir));else log(QStringLiteral("监听失败: ")+m_server->errorString());}
void MainWindow::newConnection(){while(m_server->hasPendingConnections()){auto*s=m_server->nextPendingConnection();auto*t=new FileTransfer(s,this);t->setReceiveDirectory(m_dir);m_transfers<<t;connect(t,&FileTransfer::logMessage,this,&MainWindow::log);connect(t,&FileTransfer::fileReceived,this,[this](const QString&){log(QStringLiteral("文件接收完成"));});connect(t,&FileTransfer::progress,this,[this](qint64 n,qint64 total){ui->progressBar->setMaximum(int(total>0?total:1));ui->progressBar->setValue(int(n));});connect(t,&FileTransfer::failed,this,&MainWindow::log);}}
void MainWindow::log(const QString&s){ui->logEdit->append(s);}
