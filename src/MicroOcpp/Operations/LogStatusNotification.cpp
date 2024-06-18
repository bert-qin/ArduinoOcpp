// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/LogStatusNotification.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>

using MicroOcpp::Ocpp201::LogStatusNotification;

LogStatusNotification::LogStatusNotification(DiagnosticsStatus status, int requestId) : status(status) ,requestId(requestId){
    
}

const char *LogStatusNotification::cstrFromStatus(DiagnosticsStatus status) {
    switch (status) {
        case (DiagnosticsStatus::Idle):
            return "Idle";
            break;
        case (DiagnosticsStatus::Uploaded):
            return "Uploaded";
            break;
        case (DiagnosticsStatus::UploadFailed):
            return "UploadFailure";
            break;
        case (DiagnosticsStatus::Uploading):
            return "Uploading";
            break;
        case (DiagnosticsStatus::BadMessage):
            return "BadMessage";
            break;
        case (DiagnosticsStatus::NotSupportedOperation):
            return "NotSupportedOperation";
            break;
        case (DiagnosticsStatus::PermissionDenied):
            return "PermissionDenied";
            break;
        case (DiagnosticsStatus::AcceptedCanceled):
            return "AcceptedCanceled";
            break;
    }
    return nullptr; //cannot be reached
}

std::unique_ptr<DynamicJsonDocument> LogStatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
    JsonObject payload = doc->to<JsonObject>();
    if(requestId!=-1){
        payload["requestId"] = requestId;
    }
    payload["status"] = cstrFromStatus(status);
    return doc;
}

void LogStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}

#endif