// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/ClearCache.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::ClearCache;

ClearCache::ClearCache(AuthorizationService& authService) : authService(authService){
  
}

const char* ClearCache::getOperationType(){
    return "ClearCache";
}

void ClearCache::processReq(JsonObject payload) {

    // if (!filesystem) {
    //     //no persistency - nothing to do
    //     return;
    // }

    // success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
    //     return !strncmp(fname, "sd", strlen("sd")) ||
    //            !strncmp(fname, "tx", strlen("tx")) ||
    //            !strncmp(fname, "op", strlen("op"));
    // });
    success = authService.clearAutchCache();
}

std::unique_ptr<DynamicJsonDocument> ClearCache::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (success) {
        payload["status"] = "Accepted"; //"Accepted", because the intended postcondition is true
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
