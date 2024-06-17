// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UpdateFirmware.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UpdateFirmware;

UpdateFirmware::UpdateFirmware(FirmwareService& fwService) : fwService(fwService) {

}

void UpdateFirmware::processReq(JsonObject payload) {
    const char* retrieveKey = "retrieveDate";
#if MO_ENABLE_V201    
    if(fwService.getVersion().major==2){
        if(payload.containsKey("firmware")){
            payload = payload["firmware"];
            retrieveKey = "retrieveDateTime";
        }else{
            errorCode = "FormationViolation";
            return;
        }
    }
#endif
    const char *location = payload["location"] | "";
    //check location URL. Maybe introduce Same-Origin-Policy?
    if (!*location) {
        errorCode = "FormationViolation";
        return;
    }
    
    int retries = payload["retries"] | 1;
    int retryInterval = payload["retryInterval"] | 180;
    if (retries < 0 || retryInterval < 0) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    //check the integrity of retrieveDate

    if (!payload.containsKey(retrieveKey)) {
        errorCode = "FormationViolation";
        return;
    }

    Timestamp retrieveDate;
    if (!retrieveDate.setTime(payload[retrieveKey] | "Invalid")) {
        errorCode = "PropertyConstraintViolation";
        MO_DBG_WARN("bad time format");
        return;
    }
    
    fwService.scheduleFirmwareUpdate(location, retrieveDate, (unsigned int) retries, (unsigned int) retryInterval);
}

std::unique_ptr<DynamicJsonDocument> UpdateFirmware::createConf(){
#if MO_ENABLE_V201    
    if(fwService.getVersion().major==2){
        auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Accepted";
        return doc;
    }
#endif
    return createEmptyDocument();
}
