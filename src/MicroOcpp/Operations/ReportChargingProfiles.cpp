#if MO_ENABLE_V201

#include <MicroOcpp/Operations/ReportChargingProfiles.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::ReportChargingProfiles;

ReportChargingProfiles::ReportChargingProfiles(int requestId, const char *chargingLimitSource, int evseId, std::unique_ptr<std::vector<std::unique_ptr<DynamicJsonDocument>>> chargingProfile)
    : requestId(requestId), chargingLimitSource(chargingLimitSource), evseId(evseId), chargingProfile(std::move(chargingProfile))
{
}

const char *ReportChargingProfiles::getOperationType()
{
    return "ReportChargingProfiles";
}

std::unique_ptr<DynamicJsonDocument> ReportChargingProfiles::createReq() {
    size_t capacity = 
            JSON_OBJECT_SIZE(4); //total of 4 fields

    capacity += JSON_ARRAY_SIZE(chargingProfile->size());
    for (auto& i : *chargingProfile) {
        capacity += i->memoryUsage()*2 + JSON_ARRAY_SIZE(1); // *2 + JSON_ARRAY_SIZE(1) for translate 1.6J format to 2.0.1
    }
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(capacity));

    JsonObject payload = doc->to<JsonObject>();
    payload["requestId"] = requestId;
    payload["chargingLimitSource"] = chargingLimitSource;
    payload["evseId"] = evseId;

    JsonArray array = payload.createNestedArray("chargingProfile");

    int index = 0;
    for (auto& i : *chargingProfile) {
        array.add(*i);
        index++;
    }
    return doc;
}

void ReportChargingProfiles::processConf(JsonObject payload) {
    // empty payload
}

#endif