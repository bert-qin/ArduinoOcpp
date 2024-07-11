// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTSTARTTRANSACTION_H
#define MO_REQUESTSTARTTRANSACTION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <string>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>

namespace MicroOcpp {

class Model;
class ChargingProfile;
class TransactionService;

namespace Ocpp201 {

class RequestStartTransaction : public Operation {
private:
    TransactionService& txService;
    const char *errorCode {nullptr};
    const char *errorDescription = "";
    RequestStartStopStatus status;
    char transactionId [MO_TXID_LEN_MAX + 1] = {'\0'};
public:
    RequestStartTransaction(TransactionService& txService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
