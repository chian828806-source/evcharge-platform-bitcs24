/*
 * 功能：承载 Service 层的成功数据或统一错误信息。
 * 边界：不依赖 Socket、QSqlQuery 或 UI；Handler 负责把它映射为 ResponseMessage。
 */
#pragma once

#include <QString>

#include <utility>

template<typename T>
struct ServiceResult
{
    bool ok = false;
    int code = 0;
    QString message;
    T value{};

    static ServiceResult success(T result)
    {
        return {true, 200, QStringLiteral("success"), std::move(result)};
    }

    static ServiceResult failure(int errorCode, const QString &errorMessage)
    {
        return {false, errorCode, errorMessage, {}};
    }
};
