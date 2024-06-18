// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/FirmwareStatusNotification.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

using namespace MicroOcpp;

FirmwareStatusNotification::FirmwareStatusNotification(FirmwareStatus status, int requestId) : status{status} ,requestId{requestId}{

}

const char *FirmwareStatusNotification::cstrFromFwStatus(FirmwareStatus status) {
    switch (status) {
        case (FirmwareStatus::Downloaded):
            return "Downloaded";
            break;
        case (FirmwareStatus::DownloadFailed):
            return "DownloadFailed";
            break;
        case (FirmwareStatus::Downloading):
            return "Downloading";
            break;
        case (FirmwareStatus::Idle):
            return "Idle";
            break;
        case (FirmwareStatus::InstallationFailed):
            return "InstallationFailed";
            break;
        case (FirmwareStatus::Installing):
            return "Installing";
            break;
        case (FirmwareStatus::Installed):
            return "Installed";
            break;
#if MO_ENABLE_V201
        case (FirmwareStatus::DownloadScheduled):
            return "DownloadScheduled";
            break;
        case (FirmwareStatus::DownloadPaused):
            return "DownloadPaused";
            break;
        case (FirmwareStatus::InstallRebooting):
            return "InstallRebooting";
            break;
        case (FirmwareStatus::InstallScheduled):
            return "InstallScheduled";
            break;
        case (FirmwareStatus::InstallVerificationFailed):
            return "InstallVerificationFailed";
            break;
        case (FirmwareStatus::InvalidSignature):
            return "InvalidSignature";
            break;
        case (FirmwareStatus::SignatureVerified):
            return "SignatureVerified";
            break;
# endif
    }
    return NULL; //cannot be reached
}

std::unique_ptr<DynamicJsonDocument> FirmwareStatusNotification::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
    JsonObject payload = doc->to<JsonObject>();
    if(requestId!=-1){
        payload["requestId"] = requestId;
    }
    payload["status"] = cstrFromFwStatus(status);
    return doc;
}

void FirmwareStatusNotification::processConf(JsonObject payload){
    // no payload, nothing to do
}
