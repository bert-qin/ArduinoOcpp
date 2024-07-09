// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>


#if MO_ENABLE_V201

#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/RequestStore.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>

using MicroOcpp::Ocpp201::TransactionEvent;
using namespace MicroOcpp::Ocpp201;

TransactionEvent::TransactionEvent(Model& model, std::shared_ptr<TransactionEventData> txEvent)
        : model(model), txEvent(txEvent) {

}

const char* TransactionEvent::getOperationType() {
    return "TransactionEvent";
}

void TransactionEvent::initiate(StoredOperationHandler *opStore) {
    if (!txEvent || !txEvent->transaction) {
        MO_DBG_ERR("initialization error");
        return;
    }

    auto transaction = txEvent->transaction;
    bool needCommit = false;

    if (txEvent->eventType == TransactionEventData::Type::Started) {
        if (transaction->getStartSync().isRequested()) {
            MO_DBG_ERR("initialization error");
            return;
        }

        transaction->getStartSync().setRequested();
        needCommit = true;
    }else if (txEvent->eventType == TransactionEventData::Type::Ended) {
        if (transaction->getStopSync().isRequested()) {
            MO_DBG_ERR("initialization error");
            return;
        }

        transaction->getStopSync().setRequested();
        needCommit = true;
    }else{
        // do nothing
    }

    //commit operation and tx
    if(needCommit){
        transaction->commit();
        auto payload = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(3)));
        (*payload)["connectorId"] = transaction->getConnectorId();
        (*payload)["txNr"] = transaction->getTxNr();
        (*payload)["started"] = txEvent->eventType == TransactionEventData::Type::Started; // is started or ended event

        if (opStore) {
            opStore->setPayload(std::move(payload));
            opStore->commit();
        }
    }

    MO_DBG_INFO("TransactionEvent initiated");
}

bool TransactionEvent::restore(StoredOperationHandler *opStore) {
    if (!opStore) {
        MO_DBG_ERR("invalid argument");
        return false;
    }

    auto payload = opStore->getPayload();
    if (!payload) {
        MO_DBG_ERR("memory corruption");
        return false;
    }

    int connectorId = (*payload)["connectorId"] | -1;
    int txNr = (*payload)["txNr"] | -1;
    if (connectorId < 0 || txNr < 0) {
        MO_DBG_ERR("record incomplete");
        return false;
    }

    auto txStore = model.getTransactionStore();

    if (!txStore) {
        MO_DBG_ERR("invalid state");
        return false;
    }
    auto transaction = std::static_pointer_cast<Ocpp201::Transaction>(txStore->getTransaction(connectorId, txNr));
    if (!transaction) {
        MO_DBG_ERR("referential integrity violation");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }
        return false;
    }

    if (transaction->isSilent()) {
        //transaction has been set silent after initializing StopTx - discard operation record
        MO_DBG_WARN("tx has been set silent - discard StopTx");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }
        return false;
    }

    if (transaction->getStartTimestamp() < MIN_TIME &&
            transaction->getStartBootNr() != model.getBootNr()) {
        //time not set, cannot be restored anymore -> invalid tx
        MO_DBG_ERR("cannot recover tx from previus run");

        //clean up possible tx records
        if (auto mSerivce = model.getMeteringService()) {
            mSerivce->removeTxMeterData(connectorId, txNr);
        }

        transaction->setSilent();
        transaction->setInactive();
        transaction->commit();
        return false;
    }
    bool started = (*payload)["started"] | false;
    txEvent = std::make_shared<TransactionEventData>(transaction, started?0:transaction->getSeqNo());
    if(started){
        txEvent->eventType = TransactionEventData::Type::Started;
        txEvent->timestamp = transaction->getStartTimestamp();
        txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(transaction->idToken.get()));
    }else{
        txEvent->eventType = TransactionEventData::Type::Ended;
        txEvent->timestamp = transaction->getStopTimestamp();
        txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(transaction->stopIdToken?transaction->stopIdToken->get():transaction->idToken.get()));
    }
    if (auto mSerivce = model.getMeteringService()) {
        if (auto txData = mSerivce->getStopTxMeterData(transaction.get())) {
            auto meterValue = txData->retrieveStopTxData();
            if(started){
                if(meterValue.size()>0){
                    // only use transaction begin
                    txEvent->meterValue.push_back(std::move(meterValue[0]));
                }
            }else{
                txEvent->meterValue = std::move(meterValue);
            }
        }
    }
    txEvent->offline = true;
    txEvent->triggerReason = TransactionEventTriggerReason::Trigger;
    txEvent->evse = EvseId(transaction->getConnectorId(),1);

    return true;
}

std::unique_ptr<DynamicJsonDocument> TransactionEvent::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                JSON_OBJECT_SIZE(12) + //total of 12 fields
                JSONDATE_LENGTH + 1 + //timestamp string
                JSON_OBJECT_SIZE(5) + //transactionInfo
                    MO_TXID_LEN_MAX + 1 + //transactionId
                MO_IDTOKEN_LEN_MAX + 1 //idToken
                + 512));//meterValue // todo
    JsonObject payload = doc->to<JsonObject>();

    const char *eventType = "";
    switch (txEvent->eventType) {
        case TransactionEventData::Type::Ended:
            eventType = "Ended";
            break;
        case TransactionEventData::Type::Started:
            eventType = "Started";
            break;
        case TransactionEventData::Type::Updated:
            eventType = "Updated";
            break;
    }

    payload["eventType"] = eventType;

    if(txEvent->eventType == TransactionEventData::Type::Started){
        if (txEvent->transaction->getStartTimestamp() < MIN_TIME &&
                txEvent->transaction->getStartBootNr() == model.getBootNr()) {
            MO_DBG_DEBUG("adjust preboot StartTx timestamp");
            Timestamp adjusted = model.getClock().adjustPrebootTimestamp(txEvent->transaction->getStartTimestamp());
            txEvent->transaction->setStartTimestamp(adjusted);
        }
    }else if(txEvent->eventType == TransactionEventData::Type::Ended){
            /*
        * Adjust timestamps in case they were taken before initial Clock setting
        */
        if (txEvent->transaction->getStopTimestamp() < MIN_TIME) {
            //Timestamp taken before Clock value defined. Determine timestamp
            if (txEvent->transaction->getStopBootNr() == model.getBootNr()) {
                //possible to calculate real timestamp
                Timestamp adjusted = model.getClock().adjustPrebootTimestamp(txEvent->transaction->getStopTimestamp());
                txEvent->transaction->setStopTimestamp(adjusted);
            } else if (txEvent->transaction->getStartTimestamp() >= MIN_TIME) {
                MO_DBG_WARN("set stopTime = startTime because correct time is not available");
                txEvent->transaction->setStopTimestamp(txEvent->transaction->getStartTimestamp() + 1); //1s behind startTime to keep order in backend DB
            } else {
                MO_DBG_ERR("failed to determine StopTx timestamp");
                //send invalid value
            }
        }
        // if StopTx timestamp is before StartTx timestamp, something probably went wrong. Restore reasonable temporal order
        if (txEvent->transaction->getStopTimestamp() < txEvent->transaction->getStartTimestamp()) {
            MO_DBG_WARN("set stopTime = startTime because stopTime was before startTime");
            txEvent->transaction->setStopTimestamp(txEvent->transaction->getStartTimestamp() + 1); //1s behind startTime to keep order in backend DB
        }

        for (auto mv = txEvent->meterValue.begin(); mv != txEvent->meterValue.end(); mv++) {
            if ((*mv)->getTimestamp() < MIN_TIME) {
                //time off. Try to adjust, otherwise send invalid value
                if ((*mv)->getReadingContext() == ReadingContext::TransactionBegin) {
                    (*mv)->setTimestamp(txEvent->transaction->getStartTimestamp());
                } else if ((*mv)->getReadingContext() == ReadingContext::TransactionEnd) {
                    (*mv)->setTimestamp(txEvent->transaction->getStopTimestamp());
                } else {
                    (*mv)->setTimestamp(txEvent->transaction->getStartTimestamp() + 1);
                }
            }
        }
    }else{
        // do nothing
    }
    char timestamp [JSONDATE_LENGTH + 1];
    txEvent->timestamp.toJsonString(timestamp, JSONDATE_LENGTH + 1);
    payload["timestamp"] = timestamp;

    const char *triggerReason = "";
    switch(txEvent->triggerReason) {
        case TransactionEventTriggerReason::UNDEFINED:
            MO_DBG_ERR("internal error");
            break;
        case TransactionEventTriggerReason::Authorized:
            triggerReason = "Authorized";
            break;
        case TransactionEventTriggerReason::CablePluggedIn:
            triggerReason = "CablePluggedIn";
            break;
        case TransactionEventTriggerReason::ChargingRateChanged:
            triggerReason = "ChargingRateChanged";
            break;
        case TransactionEventTriggerReason::ChargingStateChanged:
            triggerReason = "ChargingStateChanged";
            break;
        case TransactionEventTriggerReason::Deauthorized:
            triggerReason = "Deauthorized";
            break;
        case TransactionEventTriggerReason::EnergyLimitReached:
            triggerReason = "EnergyLimitReached";
            break;
        case TransactionEventTriggerReason::EVCommunicationLost:
            triggerReason = "EVCommunicationLost";
            break;
        case TransactionEventTriggerReason::EVConnectTimeout:
            triggerReason = "EVConnectTimeout";
            break;
        case TransactionEventTriggerReason::MeterValueClock:
            triggerReason = "MeterValueClock";
            break;
        case TransactionEventTriggerReason::MeterValuePeriodic:
            triggerReason = "MeterValuePeriodic";
            break;
        case TransactionEventTriggerReason::TimeLimitReached:
            triggerReason = "TimeLimitReached";
            break;
        case TransactionEventTriggerReason::Trigger:
            triggerReason = "Trigger";
            break;
        case TransactionEventTriggerReason::UnlockCommand:
            triggerReason = "UnlockCommand";
            break;
        case TransactionEventTriggerReason::StopAuthorized:
            triggerReason = "StopAuthorized";
            break;
        case TransactionEventTriggerReason::EVDeparted:
            triggerReason = "EVDeparted";
            break;
        case TransactionEventTriggerReason::EVDetected:
            triggerReason = "EVDetected";
            break;
        case TransactionEventTriggerReason::RemoteStop:
            triggerReason = "RemoteStop";
            break;
        case TransactionEventTriggerReason::RemoteStart:
            triggerReason = "RemoteStart";
            break;
        case TransactionEventTriggerReason::AbnormalCondition:
            triggerReason = "AbnormalCondition";
            break;
        case TransactionEventTriggerReason::SignedDataReceived:
            triggerReason = "SignedDataReceived";
            break;
        case TransactionEventTriggerReason::ResetCommand:
            triggerReason = "ResetCommand";
            break;
    }

    payload["triggerReason"] = triggerReason;

    payload["seqNo"] = txEvent->seqNo;

    if (txEvent->offline) {
        payload["offline"] = txEvent->offline;
    }

    if (txEvent->numberOfPhasesUsed >= 0) {
        payload["numberOfPhasesUsed"] = txEvent->numberOfPhasesUsed;
    }

    if (txEvent->cableMaxCurrent >= 0) {
        payload["cableMaxCurrent"] = txEvent->cableMaxCurrent;
    }

    if (txEvent->reservationId >= 0) {
        payload["reservationId"] = txEvent->reservationId;
    }

    JsonObject transactionInfo = payload.createNestedObject("transactionInfo");
    transactionInfo["transactionId"] = txEvent->transaction->transactionId;

    const char *chargingState = nullptr;
    switch (txEvent->chargingState) {
        case TransactionEventData::ChargingState::UNDEFINED:
            // optional, okay
            break;
        case TransactionEventData::ChargingState::Charging:
            chargingState = "Charging";
            break;
        case TransactionEventData::ChargingState::EVConnected:
            chargingState = "EVConnected";
            break;
        case TransactionEventData::ChargingState::SuspendedEV:
            chargingState = "SuspendedEV";
            break;
        case TransactionEventData::ChargingState::SuspendedEVSE:
            chargingState = "SuspendedEVSE";
            break;
        case TransactionEventData::ChargingState::Idle:
            chargingState = "Idle";
            break;
    }
    if (chargingState) { // optional
        transactionInfo["chargingState"] = chargingState;
    }

    const char *stoppedReason = nullptr;
    switch (txEvent->transaction->stopReason) {
        case Transaction::StopReason::UNDEFINED:
            // optional, okay
            break;
        case Transaction::StopReason::Local: 
            // omit reason Local
            break;
        case Transaction::StopReason::DeAuthorized: 
            stoppedReason = "DeAuthorized"; 
            break;
        case Transaction::StopReason::EmergencyStop: 
            stoppedReason = "EmergencyStop"; 
            break;
        case Transaction::StopReason::EnergyLimitReached: 
            stoppedReason = "EnergyLimitReached";
            break;
        case Transaction::StopReason::EVDisconnected: 
            stoppedReason = "EVDisconnected";
            break;
        case Transaction::StopReason::GroundFault: 
            stoppedReason = "GroundFault";
            break;
        case Transaction::StopReason::ImmediateReset:
            stoppedReason = "ImmediateReset";
            break;
        case Transaction::StopReason::LocalOutOfCredit:
            stoppedReason = "LocalOutOfCredit";
            break;
        case Transaction::StopReason::MasterPass:
            stoppedReason = "MasterPass";
            break;
        case Transaction::StopReason::Other:
            stoppedReason = "Other";
            break;
        case Transaction::StopReason::OvercurrentFault:
            stoppedReason = "OvercurrentFault";
            break;
        case Transaction::StopReason::PowerLoss:
            stoppedReason = "PowerLoss";
            break;
        case Transaction::StopReason::PowerQuality:
            stoppedReason = "PowerQuality";
            break;
        case Transaction::StopReason::Reboot:
            stoppedReason = "Reboot";
            break;
        case Transaction::StopReason::Remote:
            stoppedReason = "Remote";
            break;
        case Transaction::StopReason::SOCLimitReached:
            stoppedReason = "SOCLimitReached";
            break;
        case Transaction::StopReason::StoppedByEV:
            stoppedReason = "StoppedByEV";
            break;
        case Transaction::StopReason::TimeLimitReached:
            stoppedReason = "TimeLimitReached";
            break;
        case Transaction::StopReason::Timeout:
            stoppedReason = "Timeout";
            break;
    }
    if (stoppedReason) { // optional
        transactionInfo["stoppedReason"] = stoppedReason;
    }

    if (txEvent->remoteStartId >= 0) {
        transactionInfo["remoteStartId"] = txEvent->transaction->remoteStartId;
    }

    if (txEvent->idToken) {
        JsonObject idToken = payload.createNestedObject("idToken");
        idToken["idToken"] = txEvent->idToken->get();
        idToken["type"] = txEvent->idToken->getTypeCstr();
    }

    if (txEvent->evse.id >= 0) {
        JsonObject evse = payload.createNestedObject("evse");
        evse["id"] = txEvent->evse.id;
        if (txEvent->evse.connectorId >= 0) {
            evse["connectorId"] = txEvent->evse.connectorId;
        }
    }
    if(txEvent->meterValue.size()){
        std::vector<std::unique_ptr<DynamicJsonDocument>> entries;
        for (auto value = txEvent->meterValue.begin(); value != txEvent->meterValue.end(); value++) {
            auto entry = (*value)->toJson(VER_2_0_1);
            if (entry) {
                entries.push_back(std::move(entry));
            } else {
                MO_DBG_ERR("Energy meter reading not convertible to JSON");
            }
        }
        auto meterValueJson = payload.createNestedArray("meterValue");
        for (auto entry = entries.begin(); entry != entries.end(); entry++) {
            meterValueJson.add(**entry);
        }
    }

    return doc;
}

void TransactionEvent::processConf(JsonObject payload) {

    if (payload.containsKey("idTokenInfo")) {
        if (strcmp(payload["idTokenInfo"]["status"], "Accepted")) {
            MO_DBG_INFO("transaction deAuthorized");
            txEvent->transaction->setInactive();
            txEvent->transaction->setIdTagDeauthorized();
        }
#if MO_ENABLE_LOCAL_AUTH
        if(txEvent->eventType == TransactionEventData::Type::Started || txEvent->eventType == TransactionEventData::Type::Ended){
            if (auto authService = model.getAuthorizationService()) {
                authService->notifyAuthorization(txEvent->transaction->getIdTag(), payload["idTokenInfo"]);
            }
        }
#endif //MO_ENABLE_LOCAL_AUTH
    }
    if(txEvent->eventType == TransactionEventData::Type::Started){
        txEvent->transaction->getStartSync().confirm();
        txEvent->transaction->commit();
    }else if(txEvent->eventType == TransactionEventData::Type::Ended){
        txEvent->transaction->getStopSync().confirm();
        txEvent->transaction->commit();
    }else{
        // do nothing
    }
}

bool TransactionEvent::processErr(const char *code, const char *description, JsonObject details) {

    if (txEvent->transaction && txEvent->eventType == TransactionEventData::Type::Ended) {
        txEvent->transaction->getStopSync().confirm(); //no retry behavior for now; consider data "arrived" at server
        txEvent->transaction->commit();
    }

    MO_DBG_ERR("Server error, data loss!");

    return false;
}

void TransactionEvent::processReq(JsonObject payload) {
    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<DynamicJsonDocument> TransactionEvent::createConf() {
    return createEmptyDocument();
}

#endif // MO_ENABLE_V201
