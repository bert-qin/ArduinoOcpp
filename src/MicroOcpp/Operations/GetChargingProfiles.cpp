#if MO_ENABLE_V201

#include <MicroOcpp/Operations/GetChargingProfiles.h>
#include <MicroOcpp/Operations/ReportChargingProfiles.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <functional>

using MicroOcpp::Ocpp201::GetChargingProfiles;
using MicroOcpp::Ocpp201::ReportChargingProfiles;

GetChargingProfiles::GetChargingProfiles(SmartChargingService& scService) : scService(scService) {

}

const char* GetChargingProfiles::getOperationType(){
    return "GetChargingProfiles";
}

void GetChargingProfiles::processReq(JsonObject payload) {
    if (!payload.containsKey("requestId")) {
        errorCode = "FormationViolation";
        return;
    }
    requestId = payload["requestId"]|-1;
    if (!payload.containsKey("chargingProfile") || payload["chargingProfile"].size()<=0) {
        errorCode = "FormationViolation";
        return;
    }
    if(payload["chargingProfile"].containsKey("chargingProfileId")){
        if(!payload["chargingProfile"]["chargingProfileId"].is<JsonArray>() || payload["chargingProfile"]["chargingProfileId"].size()<=0){
            errorCode = "FormationViolation";
            return;
        }
    }
    
    std::function<bool(int, int, ChargingProfilePurposeType, int)> filter = [payload,this]
            (int chargingProfileId, int connectorId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
        auto criteria = payload["chargingProfile"];

        if (criteria.containsKey("chargingLimitSource")) {
            if(!strcmp(criteria["chargingLimitSource"], "CSO")){
                return false;
            }
        }

        if (criteria.containsKey("chargingProfileId")) {
            for (int i = 0; i < criteria["chargingProfileId"].size(); i++) {
                if (chargingProfileId == criteria["chargingProfileId"][i]) {
                    return true;
                }
                if(i == criteria["chargingProfileId"].size()-1){
                    return false;
                }
            }
        }

        if (payload.containsKey("evseId")) {
            if (connectorId != (payload["evseId"] | -1)) {
                return false;
            }
        }

        if (criteria.containsKey("chargingProfilePurpose")) {
            switch (chargingProfilePurpose) {
                case ChargingProfilePurposeType::ChargePointMaxProfile:
                    if (strcmp(criteria["chargingProfilePurpose"] | "INVALID", "ChargePointMaxProfile")) {
                        return false;
                    }
                    break;
                case ChargingProfilePurposeType::TxDefaultProfile:
                    if (strcmp(criteria["chargingProfilePurpose"] | "INVALID", "TxDefaultProfile")) {
                        return false;
                    }
                    break;
                case ChargingProfilePurposeType::TxProfile:
                    if (strcmp(criteria["chargingProfilePurpose"] | "INVALID", "TxProfile")) {
                        return false;
                    }
                    break;
                default:
                    return false;
            }
        }

        if (criteria.containsKey("stackLevel")) {
            if (stackLevel != (criteria["stackLevel"] | -1)) {
                return false;
            }
        }

        return true;
    };

    matchingProfiles = scService.getChargingProfiles(filter);
}

std::unique_ptr<DynamicJsonDocument> GetChargingProfiles::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (!matchingProfiles->empty()){
        auto notifyReport = makeRequest(new ReportChargingProfiles(
            requestId,
            "CSO",
            evseId,
            std::move(matchingProfiles)));

        scService.getContext().initiateRequest(std::move(notifyReport));
        payload["status"] = "Accepted";
    }
    else
        payload["status"] = "NoProfiles";
        
    return doc;
}

#endif