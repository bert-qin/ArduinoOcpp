// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>

using namespace MicroOcpp;

bool ITransaction::commit() {
    return context.commit(this);
}

bool Transaction::setIdTag(const char *idTag) {
    auto ret = snprintf(this->idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setParentIdTag(const char *idTag) {
    auto ret = snprintf(this->parentIdTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopIdTag(const char *idTag) {
    auto ret = snprintf(stop_idTag, IDTAG_LEN_MAX + 1, "%s", idTag);
    return ret >= 0 && ret < IDTAG_LEN_MAX + 1;
}

bool Transaction::setStopReason(const char *reason) {
    auto ret = snprintf(stop_reason, REASON_LEN_MAX + 1, "%s", reason);
    return ret >= 0 && ret < REASON_LEN_MAX + 1;
}

#if MO_ENABLE_V201

namespace MicroOcpp{
namespace Ocpp201{
bool Transaction::setTransactionIdStr(const char* transactionId) {
    auto ret = snprintf(this->transactionId, MO_TXID_LEN_MAX + 1, "%s", transactionId);
    return ret >= 0 && ret < MO_TXID_LEN_MAX + 1;
}

void Transaction::sendMeterValue(std::vector<std::unique_ptr<MeterValue>>&& meterValue){
    for(auto& i : meterValue){
        if(i->getReadingContext()==ReadingContext::SampleClock){
            clockMeterValue.push_back(std::move(i));
        }else if(i->getReadingContext()==ReadingContext::SamplePeriodic){
            periodicMeterValue.push_back(std::move(i));
        }else if(i->getReadingContext()==ReadingContext::Trigger){
            triggerMeterValue.push_back(std::move(i));
        }else{
            // do nothing
        }
    }
};

const char * Transaction::getStopIdTag(){
    if(stopIdToken && stopIdToken->get()){
        return stopIdToken->get();
    }
    return "";
}

bool Transaction::setStopIdTag(const char *idTag){
    clearAuthorized();
    if(strcmp(getIdTag(),idTag)==0){
        notifyIdToken = true;
    }else{
        stopIdToken = std::unique_ptr<IdToken>(new IdToken(idTag));
        notifyStopIdToken = true;
    }
    return true;
}

}
}
#endif

int ocpp_tx_getTransactionId(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getTransactionId();
}
bool ocpp_tx_isAuthorized(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isAuthorized();
}
bool ocpp_tx_isIdTagDeauthorized(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isIdTagDeauthorized();
}

bool ocpp_tx_isRunning(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isRunning();
}
bool ocpp_tx_isActive(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isActive();
}
bool ocpp_tx_isAborted(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isAborted();
}
bool ocpp_tx_isCompleted(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->isCompleted();
}

const char *ocpp_tx_getIdTag(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getIdTag();
}

bool ocpp_tx_getBeginTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getBeginTimestamp().toJsonString(buf, len);
}

int32_t ocpp_tx_getMeterStart(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getMeterStart();
}

bool ocpp_tx_getStartTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getStartTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopIdTag(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getStopIdTag();
}

int32_t ocpp_tx_getMeterStop(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getMeterStop();
}

void ocpp_tx_setMeterStop(OCPP_Transaction* tx, int32_t meter) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->setMeterStop(meter);
}

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getStopTimestamp().toJsonString(buf, len);
}

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx) {
    return reinterpret_cast<MicroOcpp::ITransaction*>(tx)->getStopReason();
}
