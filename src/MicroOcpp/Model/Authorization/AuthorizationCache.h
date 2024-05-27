// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZATIONCACHE_H
#define AUTHORIZATIONCACHE_H

#include <MicroOcpp/Model/Authorization/AuthorizationData.h>
#include <vector>

#ifndef MO_LocalAuthCacheMaxLength
#define MO_LocalAuthCacheMaxLength 8
#endif

namespace MicroOcpp
{

    class AuthorizationCache
    {
    private:
        std::vector<AuthorizationData> localAuthorizationCache;

    public:
        AuthorizationCache();
        ~AuthorizationCache();

        AuthorizationData *get(const char *idTag);

        bool add(const char *idTag, JsonObject idTagInfo);
        bool readJson(JsonArray localAuthorizationCache, bool compact = false); // compact: if true, then use compact non-ocpp representation
        void clear();

        size_t getJsonCapacity();
        void writeJson(JsonArray authCacheOut, bool compact = false);

        size_t size(); // used in unit tests
    };

}

#endif
