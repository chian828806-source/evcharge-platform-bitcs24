#include "passwordhasher.h"

#include <QCryptographicHash>

namespace {
QByteArray hmacSha256(const QByteArray &key, const QByteArray &message)
{
    constexpr int blockSize = 64;
    QByteArray actualKey = key;
    if (actualKey.size() > blockSize) {
        actualKey = QCryptographicHash::hash(actualKey, QCryptographicHash::Sha256);
    }
    actualKey = actualKey.leftJustified(blockSize, '\0', true);
    QByteArray outer(blockSize, char(0x5c));
    QByteArray inner(blockSize, char(0x36));
    for (int i = 0; i < blockSize; ++i) {
        outer[i] = char(outer.at(i) ^ actualKey.at(i));
        inner[i] = char(inner.at(i) ^ actualKey.at(i));
    }
    const QByteArray innerHash = QCryptographicHash::hash(inner + message, QCryptographicHash::Sha256);
    return QCryptographicHash::hash(outer + innerHash, QCryptographicHash::Sha256);
}

QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QByteArray counter(4, '\0');
    counter[3] = 1;
    QByteArray current = hmacSha256(password, salt + counter);
    QByteArray result = current;
    for (int i = 1; i < iterations; ++i) {
        current = hmacSha256(password, current);
        for (int j = 0; j < result.size(); ++j) {
            result[j] = char(result.at(j) ^ current.at(j));
        }
    }
    return result;
}

bool constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    unsigned char difference = 0;
    for (int i = 0; i < left.size(); ++i) {
        difference |= static_cast<unsigned char>(left.at(i) ^ right.at(i));
    }
    return difference == 0;
}
}

bool PasswordHasher::verifyPbkdf2Sha256(const QString &password,
                                       const QString &encodedHash)
{
    const QStringList parts = encodedHash.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != QStringLiteral("pbkdf2_sha256")) {
        return false;
    }
    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0) {
        return false;
    }
    const QByteArray salt = QByteArray::fromHex(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromHex(parts.at(3).toLatin1());
    return !salt.isEmpty() && !expected.isEmpty()
        && constantTimeEquals(pbkdf2(password.toUtf8(), salt, iterations), expected);
}
