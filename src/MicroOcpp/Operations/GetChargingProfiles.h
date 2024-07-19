#ifndef MO_GETCHARGINGPROFILES_H
#define MO_GETCHARGINGPROFILES_H

#if MO_ENABLE_V201

#include <vector>

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class SmartChargingService;

namespace Ocpp201 {

class GetChargingProfiles : public Operation {
private:
    SmartChargingService& scService;
    int requestId = -1;
    int evseId = 0;
    std::unique_ptr<std::vector<std::unique_ptr<DynamicJsonDocument>>> matchingProfiles;
    const char *errorCode {nullptr};
public:
    GetChargingProfiles(SmartChargingService& scService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif
#endif
