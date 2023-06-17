// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef STATUSNOTIFICATION_H
#define STATUSNOTIFICATION_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Core/Time.h>
#include <ArduinoOcpp/Model/ChargeControl/OcppEvseState.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class StatusNotification : public Operation {
private:
    int connectorId = 1;
    OcppEvseState currentStatus = OcppEvseState::NOT_SET;
    Timestamp timestamp;
    ErrorCode errorCode;
public:
    StatusNotification(int connectorId, OcppEvseState currentStatus, const Timestamp &timestamp, ErrorCode errorCode = nullptr);

    const char* getOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

const char *cstrFromOcppEveState(OcppEvseState state);

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
