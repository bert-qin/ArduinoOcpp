// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ClearChargingProfile.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Context.h>

#include <functional>

using MicroOcpp::Ocpp16::ClearChargingProfile;

ClearChargingProfile::ClearChargingProfile(SmartChargingService& scService) : scService(scService) {

}

const char* ClearChargingProfile::getOperationType(){
    return "ClearChargingProfile";
}

void ClearChargingProfile::processReq(JsonObject payload) {

    std::function<bool(int, int, ChargingProfilePurposeType, int)> filter = [payload,this]
            (int chargingProfileId, int connectorId, ChargingProfilePurposeType chargingProfilePurpose, int stackLevel) {
        const char* idKey = "id";
        const char* connectorIdKey = "connectorId"; 

#if MO_ENABLE_V201
        if(scService.getContext().getVersion().major==2){
            idKey = "chargingProfileId";
            connectorIdKey = "evseId";
        }
#endif
        if (payload.containsKey(idKey)) {
            if (chargingProfileId == (payload[idKey] | -1)) {
                return true;
            } else {
                return false;
            }
        }

        JsonObject criteria = payload;
#if MO_ENABLE_V201
        if(scService.getContext().getVersion().major==2){
            if (payload.containsKey("chargingProfileCriteria")) {
                criteria = payload["chargingProfileCriteria"];
            }else{
                return true;
            }
        }
#endif

        if (criteria.containsKey(connectorIdKey)) {
            if (connectorId != (criteria[connectorIdKey] | -1)) {
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
            }
        }

        if (criteria.containsKey("stackLevel")) {
            if (stackLevel != (criteria["stackLevel"] | -1)) {
                return false;
            }
        }

        return true;
    };

    matchingProfilesFound = scService.clearChargingProfile(filter);
}

std::unique_ptr<DynamicJsonDocument> ClearChargingProfile::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (matchingProfilesFound)
        payload["status"] = "Accepted";
    else
        payload["status"] = "Unknown";
        
    return doc;
}
