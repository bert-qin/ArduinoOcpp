// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUES_H
#define MO_METERVALUES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#include <vector>

namespace MicroOcpp {

class MeterValue;
class ITransaction;

namespace Ocpp16 {

class MeterValues : public Operation {
private:
    std::vector<std::unique_ptr<MeterValue>> meterValue;

    unsigned int connectorId = 0;

    std::shared_ptr<ITransaction> transaction;
    ProtocolVersion version;

public:
    MeterValues(std::vector<std::unique_ptr<MeterValue>>&& meterValue, unsigned int connectorId, std::shared_ptr<ITransaction> transaction = nullptr,const ProtocolVersion& version=VER_1_6_J);

    MeterValues(); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
