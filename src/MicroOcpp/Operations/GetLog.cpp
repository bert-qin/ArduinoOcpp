// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/GetLog.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::GetLog;

GetLog::GetLog(DiagnosticsService& diagService) : diagService(diagService) {

}

void GetLog::processReq(JsonObject payload) {
    if(!payload.containsKey("log")){
        errorCode = "FormationViolation";
        return;

    }
    JsonObject log = payload["log"];
    const char *location = log["remoteLocation"] | "";
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

    Timestamp startTime;
    if (log.containsKey("oldestTimestamp")) {
        if (!startTime.setTime(log["oldestTimestamp"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    Timestamp stopTime;
    if (log.containsKey("latestTimestamp")) {
        if (!stopTime.setTime(log["latestTimestamp"] | "Invalid")) {
            errorCode = "PropertyConstraintViolation";
            MO_DBG_WARN("bad time format");
            return;
        }
    }

    int requestId = payload["requestId"] | -1;

    fileName = diagService.requestDiagnosticsUpload(location, (unsigned int) retries, (unsigned int) retryInterval, startTime, stopTime, requestId);
}

std::unique_ptr<DynamicJsonDocument> GetLog::createConf(){
    const char* status = "Rejected";
    int capacity = JSON_OBJECT_SIZE(1)+16;
    if (!fileName.empty()) {
        capacity += JSON_OBJECT_SIZE(1) + fileName.length() + 1;
        status = "Accepted";
    }
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    payload["filename"] = fileName;
    return doc;
}

#endif