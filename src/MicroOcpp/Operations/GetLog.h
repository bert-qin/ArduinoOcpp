// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETLOG_H
#define MO_GETLOG_H

#if MO_ENABLE_V201

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class DiagnosticsService;

namespace Ocpp201 {

class GetLog : public Operation {
private:
    DiagnosticsService& diagService;
    std::string fileName;

    const char *errorCode = nullptr;
public:
    GetLog(DiagnosticsService& diagService);

    const char* getOperationType() override {return "GetLog";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif

#endif
