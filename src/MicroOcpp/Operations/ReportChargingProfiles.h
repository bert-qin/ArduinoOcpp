#ifndef MO_REPORTCHARGINGPROFILES_H
#define MO_REPORTCHARGINGPROFILES_H

#if MO_ENABLE_V201

#include <vector>

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

namespace Ocpp201 {

class ReportChargingProfiles : public Operation {
private:
    int requestId = -1;
    const char* chargingLimitSource;
    int evseId = -1;

    std::unique_ptr<std::vector<std::unique_ptr<DynamicJsonDocument>>> chargingProfile;
public:
    ReportChargingProfiles(int requestId, const char* chargingLimitSource, int evseId, std::unique_ptr<std::vector<std::unique_ptr<DynamicJsonDocument>>> chargingProfile);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;
    
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif
#endif
