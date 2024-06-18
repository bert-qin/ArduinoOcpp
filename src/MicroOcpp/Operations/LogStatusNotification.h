// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsStatus.h>

#ifndef MO_LOGSTATUSNOTIFICATION_H
#define MO_LOGSTATUSNOTIFICATION_H

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace Ocpp201 {

class LogStatusNotification : public Operation {
private:
    DiagnosticsStatus status = DiagnosticsStatus::Idle;
    int requestId = -1;
    static const char *cstrFromStatus(DiagnosticsStatus status);
public:
    LogStatusNotification(DiagnosticsStatus status,int requestId=-1);

    const char* getOperationType() override {return "LogStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

};

} //end namespace Ocpp201
} //end namespace MicroOcpp
#endif
#endif
