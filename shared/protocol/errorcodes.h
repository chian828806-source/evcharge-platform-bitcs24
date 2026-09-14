/*
 * 功能：集中定义 docs/03-API.md 已登记的公共错误码。
 * 边界：这里只保存稳定编号，不负责生成错误消息或判断业务失败原因。
 */
#pragma once

// 使用具名常量代替散落的数字，便于客户端和服务端保持一致。
namespace ErrorCodes {
constexpr int Success = 200;
constexpr int InvalidPhone = 4001;
constexpr int UserFrozen = 4002;
constexpr int InvalidSession = 4003;
constexpr int ActiveOrderExists = 4101;
constexpr int PileUnavailable = 4102;
constexpr int InsufficientBalance = 4103;
constexpr int InvalidOrderState = 4104;
constexpr int OrderCannotCancel = 4105;
constexpr int StationNotFound = 4201;
constexpr int PileNotFound = 4202;
constexpr int InvalidAdminCredentials = 4301;
constexpr int InvalidSocketMessage = 4401;
constexpr int SocketTimeout = 4402;
constexpr int PredictionNotFound = 4501;
constexpr int DatabaseError = 5001;
constexpr int InternalError = 5002;
}
