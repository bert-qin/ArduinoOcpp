// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Operations/GetLocalListVersion.h>
#include <MicroOcpp/Operations/SendLocalList.h>
#include <MicroOcpp/Operations/ClearCache.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Variables/VariableService.h>

#define MO_LOCALAUTHORIZATIONLIST_FN (MO_FILENAME_PREFIX "localauth.jsn")
#define MO_LOCALAUTHORIZATIONCACHE_BFN "authcache.jsn"
#define MO_LOCALAUTHORIZATIONCACHE_FN (MO_FILENAME_PREFIX MO_LOCALAUTHORIZATIONCACHE_BFN)
using namespace MicroOcpp;

AuthorizationService::AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {
#if MO_ENABLE_V201
    if(context.getVersion().major==2){
        auto varService = context.getModel().getVariableService();
        varService->declareVariable<bool>("AuthCtrlr", "AuthEnabled", true, MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
        varService->declareVariable<const char*>("AuthCtrlr", "MasterPassGroupId", "", MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
        localAuthListEnabledBool = varService->declareVariable<bool>("LocalAuthListCtrlr", "LocalAuthListEnabled", true);
        localAuthCacheEnabledBool = varService->declareVariable<bool>("AuthCtrlr", "AuthCacheEnabled", false);
        varService->declareVariable<bool>("LocalAuthListCtrlr", "Available", true, MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
        varService->declareVariable<int>("LocalAuthListCtrlr", "ItemsPerMessage", MO_SendLocalListMaxLength, MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
        varService->declareVariable<int>("LocalAuthListCtrlr", "BytesPerMessage", 1024, MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
    }else
#endif
    {
        localAuthListEnabledBool = declareConfiguration<bool>("LocalAuthListEnabled", true);
        localAuthCacheEnabledBool = declareConfiguration<bool>("AuthorizationCacheEnabled", false);
        declareConfiguration<int>("LocalAuthListMaxLength", MO_LocalAuthListMaxLength, CONFIGURATION_VOLATILE, true);
        declareConfiguration<int>("SendLocalListMaxLength", MO_SendLocalListMaxLength, CONFIGURATION_VOLATILE, true);
    }

    if (!localAuthListEnabledBool || !localAuthCacheEnabledBool) {
        MO_DBG_ERR("initialization error");
    }
    
    context.getOperationRegistry().registerOperation("GetLocalListVersion", [&context] () {
        return new Ocpp16::GetLocalListVersion(context.getModel());});
    context.getOperationRegistry().registerOperation("SendLocalList", [this] () {
        return new Ocpp16::SendLocalList(*this);});
    context.getOperationRegistry().registerOperation("ClearCache", [this] () {
        return new Ocpp16::ClearCache(*this);});

    loadLists();
    loadCache();
}

AuthorizationService::~AuthorizationService() {
    
}

bool AuthorizationService::loadLists() {
    if (!filesystem) {
        MO_DBG_WARN("no fs access");
        return true;
    }

    size_t msize = 0;
    if (filesystem->stat(MO_LOCALAUTHORIZATIONLIST_FN, &msize) != 0) {
        MO_DBG_DEBUG("no local authorization list stored already");
        return true;
    }
    
    auto doc = FilesystemUtils::loadJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN);
    if (!doc) {
        MO_DBG_ERR("failed to load %s", MO_LOCALAUTHORIZATIONLIST_FN);
        return false;
    }

    JsonObject root = doc->as<JsonObject>();

    int listVersion = root["listVersion"] | 0;
    
    if (!localAuthorizationList.readJson(root["localAuthorizationList"].as<JsonArray>(), listVersion, false, true)) {
        MO_DBG_ERR("list read failure");
        return false;
    }

    return true;
}
bool AuthorizationService::loadCache() {
    if (!filesystem) {
        MO_DBG_WARN("no fs access");
        return true;
    }

    size_t msize = 0;
    if (filesystem->stat(MO_LOCALAUTHORIZATIONCACHE_FN, &msize) != 0) {
        MO_DBG_DEBUG("no local authorization cache stored already");
        return true;
    }
    
    auto doc = FilesystemUtils::loadJson(filesystem, MO_LOCALAUTHORIZATIONCACHE_FN);
    if (!doc) {
        MO_DBG_ERR("failed to load %s", MO_LOCALAUTHORIZATIONCACHE_FN);
        return false;
    }

    JsonObject root = doc->as<JsonObject>();
    
    if (!localAuthorizationCache.readJson(root["localAuthorizationCache"].as<JsonArray>(), true)) {
        MO_DBG_ERR("cache read failure");
        return false;
    }

    return true;
}

AuthorizationData *AuthorizationService::getLocalAuthorization(const char *idTag) {
    AuthorizationData * authData = nullptr;
    if (localAuthListEnabledBool->getBool()) {
        authData = localAuthorizationList.get(idTag);
    }
    
    if (!authData && localAuthCacheEnabledBool->getBool()) {
        authData = localAuthorizationCache.get(idTag);
    }
    if (!authData) {
        return nullptr;
    }
    //check status
    if (authData->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
        MO_DBG_DEBUG("idTag %s local auth status %s", idTag, serializeAuthorizationStatus(authData->getAuthorizationStatus()));
        return authData;
    }

    return authData;
}

int AuthorizationService::getLocalListVersion() {
    return localAuthorizationList.getListVersion();
}

size_t AuthorizationService::getLocalListSize() {
    return localAuthorizationList.size();
}

size_t AuthorizationService::getAuthCacheSize() {
    return localAuthorizationCache.size();
}

bool AuthorizationService::updateLocalList(JsonArray localAuthorizationListJson, int listVersion, bool differential) {
    DynamicJsonDocument doc(0);
#if MO_ENABLE_V201
    if(context.getModel().getVersion().major ==2){
        doc = DynamicJsonDocument(localAuthorizationListJson.memoryUsage());
        JsonArray array = doc.to<JsonArray>();
        for (size_t i = 0; i < localAuthorizationListJson.size(); i++) {
            JsonObject item = array.createNestedObject();
            item["idTag"] = localAuthorizationListJson[i]["idToken"]["idToken"];
            if(localAuthorizationListJson[i].containsKey("idTokenInfo")){
                item["idTagInfo"] = tokenToTagInfo(localAuthorizationListJson[i]["idTokenInfo"])->as<JsonObject>();
            }    
        }
        localAuthorizationListJson = array;
    }
#endif
    bool success = localAuthorizationList.readJson(localAuthorizationListJson, listVersion, differential, false);

    if (success) {
        
        doc = DynamicJsonDocument(JSON_OBJECT_SIZE(3) +
                localAuthorizationList.getJsonCapacity());

        JsonObject root = doc.to<JsonObject>();
        root["listVersion"] = listVersion;
        JsonArray authListCompact = root.createNestedArray("localAuthorizationList");
        localAuthorizationList.writeJson(authListCompact, true);
        success = FilesystemUtils::storeJson(filesystem, MO_LOCALAUTHORIZATIONLIST_FN, doc);

        if (!success) {
            loadLists();
        }
    }

    return success;
}

bool AuthorizationService::addAutchCache(const char* idTag,JsonObject localAuthorizationCacheJson) {
    bool success = localAuthCacheEnabledBool->getBool() && localAuthorizationCache.add(idTag,localAuthorizationCacheJson);

    if (success) {
        
        DynamicJsonDocument doc (
                JSON_OBJECT_SIZE(3) +
                localAuthorizationCache.getJsonCapacity());

        JsonObject root = doc.to<JsonObject>();
        JsonArray authCacheCompact = root.createNestedArray("localAuthorizationCache");
        localAuthorizationCache.writeJson(authCacheCompact, true);
        success = FilesystemUtils::storeJson(filesystem, MO_LOCALAUTHORIZATIONCACHE_FN, doc);

        if (!success) {
            loadCache();
        }
    }

    return success;
}

bool AuthorizationService::clearAutchCache() {
    bool success = false;
    localAuthorizationCache.clear();

    success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
        return !strncmp(fname, MO_LOCALAUTHORIZATIONCACHE_BFN, strlen(fname));
    });

    if (!success) {
        loadCache();
    }

    return success;
}


void AuthorizationService::notifyAuthorization(const char *idTag, JsonObject idTagInfo) {
    //check local list conflicts and also update authorization cache.
    if(!idTagInfo.isNull())
    {
#if MO_ENABLE_V201
        if(context.getVersion().major == 2){  
            addAutchCache(idTag,tokenToTagInfo(idTagInfo)->as<JsonObject>());
            return;
        }
#endif
        addAutchCache(idTag,idTagInfo);
    }

    if (!localAuthListEnabledBool->getBool()) {
        return; //auth cache will follow
    }

    if (!idTagInfo.containsKey("status")) {
        return; //empty idTagInfo
    }

    auto localInfo = localAuthorizationList.get(idTag);
    if (!localInfo) {
        return;
    }

    //check for conflicts

    auto incomingStatus = deserializeAuthorizationStatus(idTagInfo["status"]);
    auto localStatus = localInfo->getAuthorizationStatus();

    if (incomingStatus == AuthorizationStatus::UNDEFINED) { //ignore invalid messages (handled elsewhere)
        return;
    }

    if (incomingStatus == AuthorizationStatus::ConcurrentTx) { //incoming status ConcurrentTx is equivalent to local Accepted
        incomingStatus = AuthorizationStatus::Accepted;
    }

    if (localStatus == AuthorizationStatus::Accepted && localInfo->getExpiryDate()) { //check for expiry
        auto& t_now = context.getModel().getClock().now();
        if (t_now > *localInfo->getExpiryDate()) {
            MO_DBG_DEBUG("local auth expired");
            localStatus = AuthorizationStatus::Expired;
        }
    }

    bool equivalent = true;

    if (incomingStatus != localStatus) {
        MO_DBG_WARN("local auth list status conflict");
        equivalent = false;
    }

    //check if parentIdTag definitions mismatch
    if (equivalent &&
            strcmp(localInfo->getParentIdTag() ? localInfo->getParentIdTag() : "", idTagInfo["parentIdTag"] | "")) {
        MO_DBG_WARN("local auth list parentIdTag conflict");
        equivalent = false;
    }

    MO_DBG_DEBUG("idTag %s fully evaluated: %s conflict", idTag, equivalent ? "no" : "contains");

    if (!equivalent) {
        //send error code "LocalListConflict" to server

        ChargePointStatus cpStatus = ChargePointStatus_UNDEFINED;
        if (context.getModel().getNumConnectors() > 0) {
            cpStatus = context.getModel().getConnector(0)->getStatus();
        }

        auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                    0,
                    cpStatus, //will be determined in StatusNotification::initiate
                    context.getModel().getClock().now(),
                    "LocalListConflict"));

        statusNotification->setTimeout(60000);

        context.initiateRequest(std::move(statusNotification));
    }
}

#if MO_ENABLE_V201
std::unique_ptr<DynamicJsonDocument> AuthorizationService::tokenToTagInfo(JsonObject token){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(token.memoryUsage()));
    JsonObject root = doc->to<JsonObject>();
    root["status"] = token["status"];
    if(token.containsKey("groupIdToken")){
        root["parentIdTag"] = token["groupIdToken"]["idToken"];
    }
    if(token.containsKey("cacheExpiryDateTime")){
        root["expiryDate"] = token["cacheExpiryDateTime"];
    }
    return doc;
}
#endif

#endif //MO_ENABLE_LOCAL_AUTH
