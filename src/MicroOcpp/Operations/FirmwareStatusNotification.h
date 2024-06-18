// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Operation.h>

#include <MicroOcpp/Model/FirmwareManagement/FirmwareStatus.h>

#ifndef MO_FIRMWARESTATUSNOTIFICATION_H
#define MO_FIRMWARESTATUSNOTIFICATION_H

namespace MicroOcpp {

class FirmwareStatusNotification : public Operation {
private:
    FirmwareStatus status = FirmwareStatus::Idle;
    int requestId = -1;
    static const char *cstrFromFwStatus(FirmwareStatus status);
public:
    FirmwareStatusNotification(FirmwareStatus status, int requestId=-1);

    const char* getOperationType() override {return "FirmwareStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

};

} //end namespace MicroOcpp

#endif
