// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetLocalListVersion;

GetLocalListVersion::GetLocalListVersion(Model& model) : model(model) {
  
}

const char* GetLocalListVersion::getOperationType(){
    return "GetLocalListVersion";
}

void GetLocalListVersion::processReq(JsonObject payload) {
    //empty payload
}

std::unique_ptr<DynamicJsonDocument> GetLocalListVersion::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    const char* key = "listVersion";
    int defVer = -1;
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        key = "versionNumber";
        defVer = 0;
    }
#endif
    if (auto authService = model.getAuthorizationService()) {
        payload[key] = authService->getLocalListVersion();
    } else {
        payload[key] = defVer;
    }
    return doc;
}

#endif //MO_ENABLE_LOCAL_AUTH
