// bert_qin/espocpp
// Copyright bert_qin 2023 - 2024

#include <MicroOcpp/Model/Authorization/AuthorizationCache.h>
#include <MicroOcpp/Debug.h>

#include <algorithm>
#include <numeric>

using namespace MicroOcpp;

AuthorizationCache::AuthorizationCache()
{
}

AuthorizationCache::~AuthorizationCache()
{
}

MicroOcpp::AuthorizationData *AuthorizationCache::get(const char *idTag)
{
    if (!idTag)
    {
        return nullptr;
    }
    auto it = std::find_if(localAuthorizationCache.begin(), localAuthorizationCache.end(),
                           [idTag](const AuthorizationData &data)
                           {
                               return strcasecmp(data.getIdTag(), idTag) == 0;
                           });

    if (it != localAuthorizationCache.end())
    {
        return &(*it);
    }
    else
    {
        return nullptr;
    }
}

bool AuthorizationCache::add(const char *idTag, JsonObject idTagInfo)
{
    if (!idTag)
    {
        return false;
    }
    auto it = std::find_if(localAuthorizationCache.begin(), localAuthorizationCache.end(),
                           [idTag](const AuthorizationData &data)
                           {
                               return strcasecmp(data.getIdTag(), idTag) == 0;
                           });

    if (it != localAuthorizationCache.end())
    {
        localAuthorizationCache.erase(it); // move to end
    }
    else
    {
        if (localAuthorizationCache.size() >= MO_LocalAuthCacheMaxLength)
        {
            localAuthorizationCache.erase(localAuthorizationCache.begin()); // remove first
        }
    }
    bool compact = false;
    localAuthorizationCache.push_back(AuthorizationData());

    DynamicJsonDocument doc (256);
    JsonObject root = doc.to<JsonObject>();
    root[AUTHDATA_KEY_IDTAG(compact)]=idTag;
    root[AUTHDATA_KEY_IDTAGINFO]=idTagInfo;
    localAuthorizationCache.back().readJson(root, compact);
    return true;
}

bool AuthorizationCache::readJson(JsonArray authCacheJson, bool compact)
{
    for (size_t i = 0; i < authCacheJson.size(); i++)
    {

        // check if JSON object is valid
        if (!authCacheJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAG(compact)))
        {
            return false;
        }
    }
    if (authCacheJson.size() > MO_LocalAuthCacheMaxLength)
    {
        MO_DBG_WARN("localAuthCache capacity exceeded");
        return false;
    }
    // apply new list
    localAuthorizationCache.clear();
    if (compact)
    {
        for (size_t i = 0; i < authCacheJson.size(); i++)
        {
            localAuthorizationCache.push_back(AuthorizationData());
            localAuthorizationCache.back().readJson(authCacheJson[i], compact);
        }
    }
    else
    {
        for (size_t i = 0; i < authCacheJson.size(); i++)
        {
            if (authCacheJson[i].as<JsonObject>().containsKey(AUTHDATA_KEY_IDTAGINFO))
            {
                localAuthorizationCache.push_back(AuthorizationData());
                localAuthorizationCache.back().readJson(authCacheJson[i], compact);
            }
        }
    }
    localAuthorizationCache.erase(std::remove_if(localAuthorizationCache.begin(), localAuthorizationCache.end(),
                                                 [](const AuthorizationData &elem)
                                                 {
                                                     return elem.getIdTag()[0] == '\0'; //"" means no idTag --> marked for removal
                                                 }),
                                  localAuthorizationCache.end());
    return true;
}

void AuthorizationCache::clear()
{
    localAuthorizationCache.clear();
}

size_t AuthorizationCache::getJsonCapacity()
{
    size_t res = JSON_ARRAY_SIZE(localAuthorizationCache.size());
    for (auto &entry : localAuthorizationCache)
    {
        res += entry.getJsonCapacity();
    }
    return res;
}

void AuthorizationCache::writeJson(JsonArray authCacheOut, bool compact)
{
    for (auto &entry : localAuthorizationCache)
    {
        JsonObject entryJson = authCacheOut.createNestedObject();
        entry.writeJson(entryJson, compact);
    }
}

size_t AuthorizationCache::size()
{
    return localAuthorizationCache.size();
}
