// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZATIONSERVICE_H
#define MO_AUTHORIZATIONSERVICE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Model/Authorization/AuthorizationList.h>
#include <MicroOcpp/Model/Authorization/AuthorizationCache.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Configuration.h>

namespace MicroOcpp {

class Context;

class AuthorizationService {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;
    AuthorizationList localAuthorizationList;
    AuthorizationCache localAuthorizationCache;

    std::shared_ptr<Configuration> localAuthListEnabledBool;
    std::shared_ptr<Configuration> localAuthCacheEnabledBool;

public:
    AuthorizationService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem);
    ~AuthorizationService();

    bool loadLists();
    bool loadCache();

    AuthorizationData *getLocalAuthorization(const char *idTag);

    int getLocalListVersion();
    size_t getLocalListSize(); //number of entries in current localAuthList; used in unit tests
    size_t getAuthCacheSize(); //number of entries in current localCache; used in unit tests

    bool updateLocalList(JsonArray localAuthorizationListJson, int listVersion, bool differential);
    bool addAutchCache(const char* idTag,JsonObject localAuthorizationCacheJson);
    bool clearAutchCache();

    void notifyAuthorization(const char *idTag, JsonObject idTagInfo);
};

}

#endif //MO_ENABLE_LOCAL_AUTH
#endif
