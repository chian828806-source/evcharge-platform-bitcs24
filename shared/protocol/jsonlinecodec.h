/*
 * 功能：把无边界的TCP字节流转换为按换行分隔的完整JSON帧。
 * 特性：支持半包、粘包、CRLF、空行过滤和接收缓存上限。
 */
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>

// 每个TCP连接应拥有一个独立JsonLineCodec实例。
class JsonLineCodec
{
public:
    // 防止对端一直不发送换行导致内存无限增长。
    static constexpr qsizetype MaxBufferedBytes = 2 * 1024 * 1024;

    // 追加本次收到的字节，返回其中已经完整的消息帧。
    QList<QByteArray> append(const QByteArray &bytes, bool *bufferOverflow);
    // 断线或重连时清除未完成帧。
    void clear();

    // 使用紧凑JSON并在末尾添加协议分隔符换行。
    static QByteArray encode(const QJsonObject &json);

private:
    QByteArray m_buffer;
};
