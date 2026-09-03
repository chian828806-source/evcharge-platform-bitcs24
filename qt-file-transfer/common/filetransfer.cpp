#include "filetransfer.h"
#include <QDataStream>
#include <QFileInfo>
#include <QDir>

static constexpr quint32 Magic=0x46545231; // FTR1

FileTransfer::FileTransfer(QTcpSocket *socket,QObject *parent):QObject(parent),m_socket(socket)
{
    connect(m_socket,&QTcpSocket::readyRead,this,&FileTransfer::onReadyRead);
    connect(m_socket,&QTcpSocket::bytesWritten,this,&FileTransfer::onBytesWritten);
    connect(m_socket,&QTcpSocket::disconnected,this,&FileTransfer::onDisconnected);
}

bool FileTransfer::sendFile(const QString &path)
{
    QFile input(path);
    if(!input.open(QIODevice::ReadOnly)){emit failed(QStringLiteral("无法打开文件: ")+path);return false;}
    const QFileInfo info(input);
    const QByteArray name=info.fileName().toUtf8();
    if(name.size()>65535){emit failed(QStringLiteral("文件名过长"));return false;}
    QByteArray header; QDataStream hs(&header,QIODevice::WriteOnly); hs.setByteOrder(QDataStream::BigEndian);
    hs<<Magic<<quint16(name.size())<<quint64(input.size()); header.append(name);
    m_header=header; m_sent=0; m_size=input.size();
    if(m_socket->write(header)<0){emit failed(m_socket->errorString());return false;}
    // Header is deliberately written before payload; QTcpSocket preserves ordering.
    input.seek(0); m_sent=0;
    while(!input.atEnd()){const QByteArray chunk=input.read(64*1024); if(m_socket->write(chunk)<0){emit failed(m_socket->errorString());return false;} m_sent+=chunk.size(); emit progress(m_sent,m_size);}
    m_socket->flush(); emit logMessage(QStringLiteral("已发送 %1 (%2 字节)").arg(info.fileName()).arg(m_size)); emit finished(); return true;
}

void FileTransfer::onBytesWritten(qint64){ }
void FileTransfer::onDisconnected(){emit logMessage(QStringLiteral("连接已断开"));}

bool FileTransfer::parseHeader()
{
    if(m_buffer.size()<14)return false;
    QDataStream ds(m_buffer); ds.setByteOrder(QDataStream::BigEndian); quint32 magic; quint16 n; quint64 size; ds>>magic>>n>>size;
    if(magic!=Magic||m_buffer.size()<14+n){emit failed(QStringLiteral("协议头无效"));m_buffer.clear();return false;}
    m_name=QString::fromUtf8(m_buffer.mid(14,n)); m_size=qint64(size); m_buffer.remove(0,14+n); m_received=0; m_receiving=true; return true;
}

void FileTransfer::onReadyRead()
{
    m_buffer+=m_socket->readAll();
    while(true){
        if(!m_receiving && !parseHeader()) return;
        if(!m_receiving)return;
        const qint64 need=m_size-m_received; if(need<=0){m_receiving=false; continue;}
        if(!m_file.isOpen()) { QDir().mkpath(m_receiveDirectory); m_file.setFileName(QDir(m_receiveDirectory).filePath(m_name)); if(!m_file.open(QIODevice::WriteOnly)){emit failed(QStringLiteral("无法创建接收文件"));return;} }
        const qint64 take=qMin<qint64>(need,m_buffer.size()); if(take==0)return;
        if(m_file.isOpen() && m_file.write(m_buffer.constData(),take)!=take){emit failed(QStringLiteral("保存文件失败"));return;}
        m_buffer.remove(0,take); m_received+=take; emit progress(m_received,m_size);
        if(m_received==m_size){m_file.close(); emit fileReceived(m_file.fileName()); emit logMessage(QStringLiteral("已接收 %1 (%2 字节)").arg(m_name).arg(m_size)); m_receiving=false;}
    }
}

void FileTransfer::resetReceive(){m_file.close();m_receiving=false;m_buffer.clear();}
