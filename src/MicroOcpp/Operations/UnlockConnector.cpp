// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/UnlockConnector.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::UnlockConnector;

UnlockConnector::UnlockConnector(Model& model) : model(model) {
  
}

const char* UnlockConnector::getOperationType(){
    return "UnlockConnector";
}

void UnlockConnector::processReq(JsonObject payload) {
#if MO_ENABLE_CONNECTOR_LOCK
    int connectorId = -1;

#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        connectorId = payload["evseId"] | -1;
    }else
#endif
    {
         connectorId = payload["connectorId"] | -1;
    }

    auto connector = model.getConnector(connectorId);

    if (!connector) {
        // NotSupported
        return;
    }

    unlockConnector = connector->getOnUnlockConnector();

    if (!unlockConnector) {
        // NotSupported
        return;
    }
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        auto txService = model.getTransactionService();
        if (txService && txService->getEvse(connectorId) && txService->getEvse(connectorId)->getTransaction()) {
            auto tx = txService->getEvse(eId)->getTransaction();
            if (tx->isActive()) {
                //Tx in progress. Check behavior
                    txService->getEvse(eId)->abortTransaction(Transaction::StopReason::Remote, TransactionEventTriggerReason::UnlockCommand);
            } 
        }
    }else
#endif
    {
        connector->endTransaction(nullptr, "UnlockCommand");
    }
    connector->updateTxNotification(TxNotification::RemoteStop);

    cbUnlockResult = unlockConnector();

    timerStart = mocpp_tick_ms();
#endif //MO_ENABLE_CONNECTOR_LOCK
}

std::unique_ptr<DynamicJsonDocument> UnlockConnector::createConf() {

    const char *status = "NotSupported";
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        status = "UnlockFailed";
    }
#endif

#if MO_ENABLE_CONNECTOR_LOCK
    if (unlockConnector) {

        if (mocpp_tick_ms() - timerStart < MO_UNLOCK_TIMEOUT) {
            //do poll and if more time is needed, delay creation of conf msg

            if (cbUnlockResult == UnlockConnectorResult_Pending) {
                cbUnlockResult = unlockConnector();
                if (cbUnlockResult == UnlockConnectorResult_Pending) {
                    return nullptr; //no result yet - delay confirmation response
                }
            }
        }

        if (cbUnlockResult == UnlockConnectorResult_Unlocked) {
            status = "Unlocked";
        } else {
            status = "UnlockFailed";
        }
    }
#endif //MO_ENABLE_CONNECTOR_LOCK

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}
