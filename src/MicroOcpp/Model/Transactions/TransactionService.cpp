// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <string.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Operations/RequestStopTransaction.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Core/Connection.h>

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionService::Evse::Evse(Context& context, TransactionService& txService, unsigned int evseId) :
        context(context), model(context.getModel()), connector(model.getConnector(evseId)), txService(txService), evseId(evseId) {
}

std::shared_ptr<Ocpp201::Transaction> TransactionService::Evse::allocateTransaction() {
    return std::static_pointer_cast<Ocpp201::Transaction>(connector->allocateTransaction());
}

void TransactionService::Evse::loop() {
    auto transaction = std::static_pointer_cast<Ocpp201::Transaction>(connector->getTransaction()); 
    // tx-related behavior
    if (transaction) {
        if (connectorPluggedInput) {
            if (connectorPluggedInput()) {
                // if cable has been plugged at least once, EVConnectionTimeout will never get triggered
                transaction->evConnectionTimeoutListen = false;
            }

            if (transaction->isActive() &&
                    transaction->evConnectionTimeoutListen &&
                    transaction->getBeginTimestamp() > MIN_TIME &&
                    txService.evConnectionTimeOutInt && txService.evConnectionTimeOutInt->getInt() > 0 &&
                    !connectorPluggedInput() &&
                    context.getModel().getClock().now() - transaction->getBeginTimestamp() >= txService.evConnectionTimeOutInt->getInt()) {

                MO_DBG_INFO("Session mngt: timeout");
                transaction->setInactive();
                transaction->commit();
                connector->updateTxNotification(TxNotification::ConnectionTimeout);
                transaction->stopTrigger = TransactionEventTriggerReason::EVConnectTimeout;
                transaction->stopReason = Ocpp201::Transaction::StopReason::Timeout;
            }
        }
    }
    std::shared_ptr<TransactionEventData> txEvent;
    bool txStopCondition = false;

    {
        // stop tx?

        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;
        Ocpp201::Transaction::StopReason stopReason = Ocpp201::Transaction::StopReason::UNDEFINED;

        if (transaction && !transaction->isActive()) {
            // tx ended via endTransaction
            txStopCondition = true;
            triggerReason = transaction->stopTrigger;
            stopReason = transaction->stopReason;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::EVConnected) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    connectorPluggedInput && !connectorPluggedInput() &&
                    (txService.stopTxOnEVSideDisconnectBool->getBool() || !transaction || !transaction->getStartSync().isRequested())) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
            stopReason = Ocpp201::Transaction::StopReason::EVDisconnected;
        } else if ((txService.isTxStopPoint(TxStartStopPoint::Authorized) ||
                    txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) &&
                    (!transaction || !transaction->isAuthorized())) {
            // user revoked authorization (or EV or any "local" entity)
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::StopAuthorized;
            stopReason = Ocpp201::Transaction::StopReason::Local;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    evReadyInput && !evReadyInput()) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            stopReason = Ocpp201::Transaction::StopReason::StoppedByEV;
        } else if (txService.isTxStopPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    !(evReadyInput && evReadyInput()) &&
                    !(evseReadyInput && evseReadyInput())) {
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            stopReason = Ocpp201::Transaction::StopReason::Other;
        } else if (txService.isTxStopPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isIdTagDeauthorized() &&
                    txService.stopTxOnInvalidIdBool->getBool()) {
            // OCPP server rejected authorization
            txStopCondition = true;
            triggerReason = TransactionEventTriggerReason::Deauthorized;
            stopReason = Ocpp201::Transaction::StopReason::DeAuthorized;
        }

        if (txStopCondition &&
                transaction && transaction->getStartSync().isRequested() && transaction->isActive()) {

            MO_DBG_INFO("Session mngt: TxStopPoint reached");
            transaction->setInactive();
            transaction->stopTrigger = triggerReason;
            transaction->stopReason = stopReason;
        }

        if (transaction &&
                transaction->getStartSync().isRequested() && !transaction->getStopSync().isRequested() && !transaction->isActive() &&
                (!stopTxReadyInput || stopTxReadyInput())) {
            // yes, stop running tx

            if (transaction->getStopTimestamp() <= MIN_TIME) {
                transaction->setStopTimestamp(model.getClock().now());
                transaction->setStopBootNr(model.getBootNr());
            }

            transaction->getStopSync().setRequested();
            transaction->getStopSync().setOpNr(context.getRequestQueue().getNextOpNr());

            if (transaction->isSilent()) {
                MO_DBG_INFO("silent Transaction: omit StopTx");
                connector->updateTxNotification(TxNotification::StopTx);
                transaction->getStopSync().confirm();
                if (auto mService = model.getMeteringService()) {
                    mService->removeTxMeterData(evseId, transaction->getTxNr());
                }
                model.getTransactionStore()->remove(evseId, transaction->getTxNr());
                transaction = nullptr;
                return;
            }
            
            transaction->commit();
            connector->updateTxNotification(TxNotification::StopTx);

            txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
            if (!txEvent) {
                // OOM
                return;
            }

            transaction->stopTrigger = triggerReason;
            transaction->stopReason = stopReason;

            txEvent->eventType = TransactionEventData::Type::Ended;
            txEvent->triggerReason = triggerReason;

            auto meteringService = context.getModel().getMeteringService();
            if (meteringService) {
                std::shared_ptr<TransactionMeterData> stopTxData = meteringService->endTxMeterData(transaction.get());
                if(stopTxData){
                    txEvent->meterValue = std::move(stopTxData->retrieveStopTxData());
                }
            } else {
                MO_DBG_ERR("MeterStart undefined");
            }
        }
    } 
    
    if (!txStopCondition) {
        // start tx?
        bool txStartCondition = false;
        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;
        // tx should be started?
        if (txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed) &&
                    (!connectorPluggedInput || connectorPluggedInput()) &&
                    transaction && transaction->isAuthorized()) {
            txStartCondition = true;
            transaction->trackAuthorized = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::Authorized;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::Authorized) &&
                    transaction && transaction->isAuthorized()) {
            txStartCondition = true;
            transaction->trackAuthorized = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::Authorized;
            }
        } else if (txService.isTxStartPoint(TxStartStopPoint::EVConnected) &&
                    connectorPluggedInput && connectorPluggedInput()) {
            txStartCondition = true;
            triggerReason = TransactionEventTriggerReason::CablePluggedIn;
        } else if (txService.isTxStartPoint(TxStartStopPoint::EnergyTransfer) &&
                    (evReadyInput || evseReadyInput) && // at least one of the two defined
                    (!evReadyInput || evReadyInput()) &&
                    (!evseReadyInput || evseReadyInput())) {
            txStartCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
        }

        if (txStartCondition &&
                (!transaction || (transaction->isActive() && !transaction->getStartSync().isRequested())) &&
                (!startTxReadyInput || startTxReadyInput())) {
            // start tx
            
            if (!transaction) {
                transaction = allocateTransaction();
                if (!transaction) {
                    // OOM
                    return;
                }
                connector->setTransaction(transaction);
            }
            if (evseId > 0) {
                transaction->setConnectorId(evseId);
            }
            
            if (transaction->getStartTimestamp() <= MIN_TIME) {
                transaction->setStartTimestamp(model.getClock().now());
                transaction->setStartBootNr(model.getBootNr());
            }

            transaction->getStartSync().setRequested();
            transaction->getStartSync().setOpNr(context.getRequestQueue().getNextOpNr());

            connector->updateTxNotification(TxNotification::StartTx);

            if (transaction->isSilent()) {
                MO_DBG_INFO("silent Transaction: omit StartTx");
                transaction->getStartSync().confirm();
                transaction->commit();
                return;
            }
            transaction->commit();
            txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Started;
            txEvent->triggerReason = triggerReason;

            auto meteringService = context.getModel().getMeteringService();
            if (meteringService) {
                meteringService->beginTxMeterData(transaction.get());
                auto value = meteringService->takeBeginMeterValue(evseId);
                if(value){
                    std::vector<std::unique_ptr<MeterValue>> data;
                    data.push_back(std::move(value));
                    txEvent->meterValue = std::move(data);
                }
            } else {
                MO_DBG_ERR("MeterStart undefined");
            }
        }
    }

    TransactionEventData::ChargingState chargingState = TransactionEventData::ChargingState::Idle;
    if (connectorPluggedInput && !connectorPluggedInput()) {
        chargingState = TransactionEventData::ChargingState::Idle;
    } else if (!transaction || !transaction->isAuthorized()) {
        chargingState = TransactionEventData::ChargingState::EVConnected;
    } else if (evseReadyInput && !evseReadyInput()) { 
        chargingState = TransactionEventData::ChargingState::SuspendedEVSE;
    } else if (evReadyInput && !evReadyInput()) {
        chargingState = TransactionEventData::ChargingState::SuspendedEV;
    } else if (ocppPermitsCharge()) {
        chargingState = TransactionEventData::ChargingState::Charging;
    }

    std::vector<std::unique_ptr<MeterValue>>* meterValue = nullptr;

    if (transaction) {
        // update tx?

        bool txUpdateCondition = false;

        TransactionEventTriggerReason triggerReason = TransactionEventTriggerReason::UNDEFINED;

        if (chargingState != trackChargingState) {
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::ChargingStateChanged;
            transaction->notifyChargingState = true;
            trackChargingState = chargingState;
        }else if (transaction->isAuthorized() && !transaction->trackAuthorized) {
            transaction->trackAuthorized = true;
            txUpdateCondition = true;
            if (transaction->remoteStartId >= 0) {
                triggerReason = TransactionEventTriggerReason::RemoteStart;
            } else {
                triggerReason = TransactionEventTriggerReason::Authorized;
            }
        } else if (connectorPluggedInput && connectorPluggedInput() && !transaction->trackEvConnected) {
            transaction->trackEvConnected = true;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::CablePluggedIn;
        } else if (connectorPluggedInput && !connectorPluggedInput() && transaction->trackEvConnected) {
            transaction->trackEvConnected = false;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::EVCommunicationLost;
        } else if (!transaction->isAuthorized() && transaction->trackAuthorized) {
            transaction->trackAuthorized = false;
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::StopAuthorized;
        } else if (evReadyInput && evReadyInput() && !transaction->trackPowerPathClosed) {
            transaction->trackPowerPathClosed = true;
        } else if (evReadyInput && !evReadyInput() && transaction->trackPowerPathClosed) {
            transaction->trackPowerPathClosed = false;
        } else if(transaction->periodicMeterValue.size()){
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::MeterValuePeriodic;
            meterValue = &(transaction->periodicMeterValue);
            transaction->notifyMeterValue = true;
        } else if(transaction->clockMeterValue.size()){
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::MeterValueClock;
            meterValue = &(transaction->clockMeterValue);
            transaction->notifyMeterValue = true;
        } else if(transaction->triggerMeterValue.size()){
            txUpdateCondition = true;
            triggerReason = TransactionEventTriggerReason::Trigger;
            meterValue = &(transaction->triggerMeterValue);
            transaction->notifyMeterValue = true;
        }

        if (txUpdateCondition && !txEvent && transaction->isRunning()) {
            // yes, updated

            txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
            if (!txEvent) {
                // OOM
                return;
            }

            txEvent->eventType = TransactionEventData::Type::Updated;
            txEvent->triggerReason = triggerReason;
        }
    }

    if (txEvent) {
        txEvent->timestamp = context.getModel().getClock().now();
        txEvent->offline = !context.getConnection().isConnected();
        if (transaction->notifyChargingState) {
            txEvent->chargingState = chargingState;
            transaction->notifyChargingState = false;
        }
        if (transaction->notifyEvseId) {
            txEvent->evse = EvseId(evseId, 1);
            transaction->notifyEvseId = false;
        }
        if (transaction->notifyRemoteStartId) {
            txEvent->remoteStartId = transaction->remoteStartId;
            transaction->notifyRemoteStartId = false;
        }
        if(transaction->notifyMeterValue){
            txEvent->meterValue = std::move(*meterValue);
            transaction->notifyMeterValue = false;
        }

        if (transaction->notifyStopIdToken && transaction->stopIdToken) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(*transaction->stopIdToken.get()));
            transaction->notifyStopIdToken = false;
        }  else if (transaction->notifyIdToken) {
            txEvent->idToken = std::unique_ptr<IdToken>(new IdToken(transaction->idToken));
            transaction->notifyIdToken = false;
        }
    }

    if (txEvent) {
        auto txEventRequest = makeRequest(new Ocpp201::TransactionEvent(context.getModel(), txEvent));
        txEventRequest->setTimeout(0);
        context.initiateRequest(std::move(txEventRequest));
    }
}

void TransactionService::Evse::setConnectorPluggedInput(std::function<bool()> connectorPlugged) {
    this->connectorPluggedInput = connectorPlugged;
}

void TransactionService::Evse::setEvReadyInput(std::function<bool()> evRequestsEnergy) {
    this->evReadyInput = evRequestsEnergy;
}

void TransactionService::Evse::setEvseReadyInput(std::function<bool()> connectorEnergized) {
    this->evseReadyInput = connectorEnergized;
}

bool TransactionService::Evse::beginAuthorization(IdToken idToken, IdToken groupIdToken, bool validateIdToken) {
    std::shared_ptr<ITransaction> transaction;
    if (validateIdToken) {
        transaction = connector->beginTransaction(idToken.get());
    } else {
        transaction = connector->beginTransaction_authorized(idToken.get(),groupIdToken.get());
    }

    return transaction!=nullptr;
}
bool TransactionService::Evse::endAuthorization(IdToken idToken, bool validateIdToken) {
    auto transaction = std::static_pointer_cast<Ocpp201::Transaction>(connector->getTransaction()); 
    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return false;
    }

    if(validateIdToken){
        connector->endTransaction(idToken.get(),nullptr);
        transaction->stopTrigger = TransactionEventTriggerReason::StopAuthorized;
        transaction->stopReason = Ocpp201::Transaction::StopReason::Local;
    }else{
        connector->endTransaction_authorized(idToken.get(),nullptr);
        transaction->stopTrigger = TransactionEventTriggerReason::AbnormalCondition;
        transaction->stopReason = Ocpp201::Transaction::StopReason::Reboot;
    }
    return true;
}
bool TransactionService::Evse::abortTransaction(Ocpp201::Transaction::StopReason stopReason, TransactionEventTriggerReason stopTrigger) {
    auto transaction = std::static_pointer_cast<Ocpp201::Transaction>(connector->getTransaction()); 
    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("Abort session started by idTag %s",
                            transaction->idToken.get());

    transaction->setInactive();
    transaction->commit();
    transaction->stopTrigger = stopTrigger;
    transaction->stopReason = stopReason;

    return true;
}
std::shared_ptr<MicroOcpp::ITransaction>& TransactionService::Evse::getTransaction() {
    return connector->getTransaction();
}

void TransactionService::Evse::setTransaction(std::shared_ptr<ITransaction> transaction) {
    return connector->setTransaction(transaction);
}

bool TransactionService::Evse::ocppPermitsCharge() {
    return connector->ocppPermitsCharge();
}

bool TransactionService::isTxStartPoint(TxStartStopPoint check) {
    for (auto& v : txStartPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}
bool TransactionService::isTxStopPoint(TxStartStopPoint check) {
    for (auto& v : txStopPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}

bool TransactionService::parseTxStartStopPoint(const char *csl, std::vector<TxStartStopPoint>& dst) {
    dst.clear();

    while (*csl == ',') {
        csl++;
    }

    while (*csl) {
        if (!strncmp(csl, "ParkingBayOccupancy", sizeof("ParkingBayOccupancy") - 1)
                && (csl[sizeof("ParkingBayOccupancy") - 1] == '\0' || csl[sizeof("ParkingBayOccupancy") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::ParkingBayOccupancy);
            csl += sizeof("ParkingBayOccupancy") - 1;
        } else if (!strncmp(csl, "EVConnected", sizeof("EVConnected") - 1)
                && (csl[sizeof("EVConnected") - 1] == '\0' || csl[sizeof("EVConnected") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EVConnected);
            csl += sizeof("EVConnected") - 1;
        } else if (!strncmp(csl, "Authorized", sizeof("Authorized") - 1)
                && (csl[sizeof("Authorized") - 1] == '\0' || csl[sizeof("Authorized") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::Authorized);
            csl += sizeof("Authorized") - 1;
        } else if (!strncmp(csl, "DataSigned", sizeof("DataSigned") - 1)
                && (csl[sizeof("DataSigned") - 1] == '\0' || csl[sizeof("DataSigned") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::DataSigned);
            csl += sizeof("DataSigned") - 1;
        } else if (!strncmp(csl, "PowerPathClosed", sizeof("PowerPathClosed") - 1)
                && (csl[sizeof("PowerPathClosed") - 1] == '\0' || csl[sizeof("PowerPathClosed") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::PowerPathClosed);
            csl += sizeof("PowerPathClosed") - 1;
        } else if (!strncmp(csl, "EnergyTransfer", sizeof("EnergyTransfer") - 1)
                && (csl[sizeof("EnergyTransfer") - 1] == '\0' || csl[sizeof("EnergyTransfer") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EnergyTransfer);
            csl += sizeof("EnergyTransfer") - 1;
        } else {
            MO_DBG_ERR("unkown TxStartStopPoint");
            dst.clear();
            return false;
        }

        while (*csl == ',') {
            csl++;
        }
    }

    return true;
}

TransactionService::TransactionService(Context& context) : context(context) {
    auto variableService = context.getModel().getVariableService();

    txStartPointString = variableService->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "PowerPathClosed");
    txStopPointString  = variableService->declareVariable<const char*>("TxCtrlr", "TxStopPoint",  "PowerPathClosed");
    stopTxOnInvalidIdBool = variableService->declareVariable<bool>("TxCtrlr", "StopTxOnInvalidId", true);
    stopTxOnEVSideDisconnectBool = variableService->declareVariable<bool>("TxCtrlr", "StopTxOnEVSideDisconnect", true);
    evConnectionTimeOutInt = variableService->declareVariable<int>("TxCtrlr", "EVConnectionTimeOut", 30);
    authorizeRemoteStartBool = variableService->declareVariable<bool>("AuthCtrlr", "AuthorizeRemoteStart", false);

    variableService->registerValidator<const char*>("TxCtrlr", "TxStartPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    variableService->registerValidator<const char*>("TxCtrlr", "TxStopPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    evses.reserve(MO_NUM_EVSE);

    for (unsigned int evseId = 0; evseId < MO_NUM_EVSE; evseId++) {
        evses.emplace_back(context, *this, evseId);
    }

    //make sure EVSE 0 will only trigger transactions if TxStartPoint is Authorized
    evses[0].connectorPluggedInput = [] () {return false;};
    evses[0].evReadyInput = [] () {return false;};
    evses[0].evseReadyInput = [] () {return false;};

    context.getOperationRegistry().registerOperation("RequestStartTransaction", [this] () {
        return new RequestStartTransaction(*this);});
    context.getOperationRegistry().registerOperation("RequestStopTransaction", [this] () {
        return new RequestStopTransaction(*this);});
}

void TransactionService::loop() {
    for (Evse& evse : evses) {
        evse.loop();
    }

    if (txStartPointString->getWriteCount() != trackTxStartPoint) {
        parseTxStartStopPoint(txStartPointString->getString(), txStartPointParsed);
    }

    if (txStopPointString->getWriteCount() != trackTxStopPoint) {
        parseTxStartStopPoint(txStopPointString->getString(), txStopPointParsed);
    }

    // assign tx on evseId 0 to an EVSE
    if (auto& tx0 = evses[0].getTransaction()) {
        //pending tx on evseId 0
        if (tx0->isActive()) {
            for (unsigned int evseId = 1; evseId < MO_NUM_EVSE; evseId++) {
                if (!evses[evseId].getTransaction() && 
                        (!evses[evseId].connectorPluggedInput || evses[evseId].connectorPluggedInput())) {
                    MO_DBG_INFO("assign tx to evse %u", evseId);
                    tx0->setConnectorId(evseId);
                    evses[evseId].setTransaction(std::move(tx0));
                }
            }
        }
    }
}

TransactionService::Evse *TransactionService::getEvse(unsigned int evseId) {
    if (evseId < evses.size()) {
        return &evses[evseId];
    } else {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
}

RequestStartStopStatus TransactionService::requestStartTransaction(unsigned int evseId, unsigned int remoteStartId, IdToken idToken, char *transactionIdOut,  IdToken groupIdToken, int chargingProfileId) {
    auto evse = getEvse(evseId);
    if (!evse) {
        return RequestStartStopStatus_Rejected;
    }

    auto tx = evse->getTransaction();
    if (tx) {
        auto ret = snprintf(transactionIdOut, MO_TXID_LEN_MAX + 1, "%s", tx->getTransactionIdStr());
        if (ret < 0 || ret >= MO_TXID_LEN_MAX + 1) {
            MO_DBG_ERR("internal error");
            return RequestStartStopStatus_Rejected;
        }
    }

    if (!evse->beginAuthorization(idToken, groupIdToken, authorizeRemoteStartBool && authorizeRemoteStartBool->getBool())) {
        MO_DBG_INFO("EVSE still occupied with pending tx");
        return RequestStartStopStatus_Rejected;
    }

    tx = evse->getTransaction();
    if (!tx) {
        MO_DBG_ERR("internal error");
        return RequestStartStopStatus_Rejected;
    }

    tx->setRemoteStartId(remoteStartId);
    if (chargingProfileId >= 0) {
        tx->setTxProfileId(chargingProfileId);
    }
    evse->getConnector()->updateTxNotification(TxNotification::RemoteStart);

    return RequestStartStopStatus_Accepted;
}

RequestStartStopStatus TransactionService::requestStopTransaction(const char *transactionId) {
    bool success = false;

    for (Evse& evse : evses) {
        if (evse.getTransaction() && !strcmp(evse.getTransaction()->getTransactionIdStr(), transactionId)) {
            success = evse.abortTransaction(Ocpp201::Transaction::StopReason::Remote, TransactionEventTriggerReason::RemoteStop);
            break;
        }
    }

    return success ?
            RequestStartStopStatus_Accepted :
            RequestStartStopStatus_Rejected;
}

#endif // MO_ENABLE_V201
