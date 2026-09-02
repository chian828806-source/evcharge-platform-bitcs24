#pragma once

#include <QString>

namespace PasswordHasher {
bool verifyPbkdf2Sha256(const QString &password, const QString &encodedHash);
}
