/*
 * 功能：实现JSON Lines的增量分帧与编码。
 * 输入可能是半条消息，也可能一次包含多条消息。
 */
#include "jsonlinecodec.h"

#include <QJsonDocument>

QList<QByteArray> JsonLineCodec::append(const QByteArray &bytes,
                                        bool *bufferOverflow)
{
    // 每次调用都向调用方返回明确的溢出状态。
    if (bufferOverflow) {
        *bufferOverflow = false;
    }
    m_buffer.append(bytes);

    // 没有换行且缓存超限，说明对端没有按协议结束消息。
    if (m_buffer.size() > MaxBufferedBytes && !m_buffer.contains('\n')) {
        m_buffer.clear();
        if (bufferOverflow) {
            *bufferOverflow = true;
        }
        return {};
    }

    // 循环取出完整行；最后不足一行的数据留到下一次append。
    QList<QByteArray> frames;
    qsizetype newlineIndex = -1;
    while ((newlineIndex = m_buffer.indexOf('\n')) >= 0) {
        QByteArray frame = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);
        // 兼容发送方使用Windows风格CRLF。
        if (frame.endsWith('\r')) {
            frame.chop(1);
        }
        // 空行不构成业务消息。
        if (frame.isEmpty()) {
            continue;
        }
        // 找到换行后仍需限制单帧自身大小。
        if (frame.size() > MaxBufferedBytes) {
            if (bufferOverflow) {
                *bufferOverflow = true;
            }
            frames.clear();
            m_buffer.clear();
            return frames;
        }
        frames.append(frame);
    }
    // 即使本次数据前部包含完整帧，尾部未完成帧仍必须受2 MiB上限约束。
    if (m_buffer.size() > MaxBufferedBytes) {
        m_buffer.clear();
        frames.clear();
        if (bufferOverflow) {
            *bufferOverflow = true;
        }
    }
    return frames;
}

void JsonLineCodec::clear()
{
    // Socket生命周期结束时丢弃残留半帧。
    m_buffer.clear();
}

QByteArray JsonLineCodec::encode(const QJsonObject &json)
{
    // Compact模式不插入格式化换行，保证一行一条消息。
    return QJsonDocument(json).toJson(QJsonDocument::Compact) + '\n';
}
