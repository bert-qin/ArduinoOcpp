// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCACHE_H
#define CLEARCACHE_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class AuthorizationService;

namespace Ocpp16 {

class ClearCache : public Operation {
private:
    AuthorizationService& authService;
    bool success = true;
public:
    ClearCache(AuthorizationService& authService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
