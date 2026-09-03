#pragma once
#include "network/messagedispatcher.h"
class PredictionService;
class PredictionHandler
{
public:
    explicit PredictionHandler(PredictionService *service);
    ResponseMessage list(const RequestMessage &, const SessionContext &);
    ResponseMessage warning(const RequestMessage &, const SessionContext &);
private: PredictionService *m_service;
};
