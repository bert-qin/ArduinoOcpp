// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef TRANSACTION_H
#define TRANSACTION_H

#ifdef __cplusplus

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>

namespace MicroOcpp {

/*
 * A transaction is initiated by the client (charging station) and processed by the server (central system).
 * The client side of a transaction is all data that is generated or collected at the charging station. The
 * server side is all transaction data that is assigned by the central system.
 * 
 * See OCPP 1.6 Specification - Edition 2, sections 3.6, 4.8, 4.10 and 5.11. 
 */

class ConnectorTransactionStore;

class SendStatus {
private:
    bool requested = false;
    bool confirmed = false;
public:
    void setRequested() {this->requested = true;}
    bool isRequested() {return requested;}
    void confirm() {confirmed = true;}
    bool isConfirmed() {return confirmed;}
};

class ITransaction{
public:
    ITransaction(ConnectorTransactionStore& context, unsigned int connectorId, unsigned int txNr, bool silent = false) : 
                context(context),
                connectorId(connectorId), 
                txNr(txNr),
                silent(silent) {}

    virtual ~ITransaction()=default;
    // common
    /*
     * data assigned by OCPP server
     */
    bool isAuthorized() {return authorized;} //Authorize has been accepted
    bool isIdTagDeauthorized() {return deauthorized;} //StartTransaction has been rejected
        /*
     * Transaction life cycle
     */
    bool isRunning() {return start_sync.isRequested() && !stop_sync.isRequested();} //tx is running
    bool isActive() {return active;} //tx continues to run or is preparing
    bool isAborted() {return !start_sync.isRequested() && !active;} //tx ended before startTx was sent
    bool isCompleted() {return stop_sync.isConfirmed();} //tx ended and startTx and stopTx have been confirmed by server
    /*
     * After modifying a field of tx, commit to make the data persistent
     */
    bool commit();
    void setInactive() {active = false;}
    virtual void setAuthorized() {authorized = true;}
    void clearAuthorized() {authorized = false;}
    void setIdTagDeauthorized() {deauthorized = true;}

    const Timestamp& getBeginTimestamp() {return begin_timestamp;}
    void setBeginTimestamp(Timestamp timestamp) {begin_timestamp = timestamp;}

    int getReservationId() {return reservationId;}
    void setReservationId(int reservationId) {this->reservationId = reservationId;}

    int getTxProfileId() {return txProfileId;}
    void setTxProfileId(int txProfileId) {this->txProfileId = txProfileId;}

    SendStatus& getStartSync() {return start_sync;}

    const Timestamp& getStartTimestamp() {return start_timestamp;}
    void setStartTimestamp(Timestamp timestamp) {start_timestamp = timestamp;}

    uint16_t getStartBootNr() {return start_bootNr;}
    void setStartBootNr(uint16_t bootNr) {start_bootNr = bootNr;} 

    SendStatus& getStopSync() {return stop_sync;}

    void setStopTimestamp(Timestamp timestamp) {stop_timestamp = timestamp;}
    const Timestamp& getStopTimestamp() {return stop_timestamp;}

    void setStopBootNr(uint16_t bootNr) {stop_bootNr = bootNr;} 
    uint16_t getStopBootNr() {return stop_bootNr;}

    // in ocpp2.0.1 is evseId
    virtual void setConnectorId(unsigned int connectorId) {this->connectorId = connectorId;}
    unsigned int getConnectorId() {return connectorId;}

    void setTxNr(unsigned int txNr) {this->txNr = txNr;}
    unsigned int getTxNr() {return txNr;} //internal primary key of this tx object

    void setSilent() {silent = true;}
    bool isSilent() {return silent;} //no data will be sent to server and server will not assign transactionId
    
    // ocpp1.6j
    virtual int getTransactionId() {return 0;}
    virtual void setTransactionId(int transactionId) {}

    virtual const char *getIdTag() {return "";}
    virtual bool setIdTag(const char *idTag){return false;}

    virtual const char *getParentIdTag() {return "";}
    virtual bool setParentIdTag(const char *idTag){return false;}

    virtual int32_t getMeterStart() {return 0;}
    virtual bool isMeterStartDefined() {return false;}
    virtual void setMeterStart(int32_t meter){}

    virtual int32_t getMeterStop() {return 0;}
    virtual bool isMeterStopDefined() {return false;}
    virtual void setMeterStop(int32_t meter) {}

    virtual const char *getStopReason() {return "";}
    virtual bool setStopReason(const char *reason){return false;}

    virtual const char *getStopIdTag() {return "";}
    virtual bool setStopIdTag(const char *idTag) {return false;}

    // ocpp2.0.1
    virtual const char* getTransactionIdStr() {return "";}
    virtual bool setTransactionIdStr(const char* transactionId) {return false;}
    virtual void sendMeterValue(std::vector<std::unique_ptr<MeterValue>>&& meterValue){}
    virtual int getRemoteStartId() {return -1;}
    virtual void setRemoteStartId(int id) {}

protected:
    ConnectorTransactionStore& context;
        /*
     * General attributes
     */
    unsigned int connectorId = 0;
    unsigned int txNr = 0; //client-side key of this tx object (!= transactionId)

    bool silent = false; //silent Tx: process tx locally, without reporting to the server
    bool active = true; //once active is false, the tx must stop (or cannot start at all)

    bool authorized = false;    //if the given idTag was authorized
    bool deauthorized = false;  //if the server revoked a local authorization
    Timestamp begin_timestamp = MIN_TIME;
    int reservationId = -1;
    int txProfileId = -1;

    SendStatus start_sync;
    Timestamp start_timestamp = MIN_TIME;      //timestamp of StartTx; can be set before actually initiating
    uint16_t start_bootNr = 0;

    SendStatus stop_sync;
    Timestamp stop_timestamp = MIN_TIME;
    uint16_t stop_bootNr = 0;
};

class Transaction : public ITransaction{
private:
    /*
     * Attributes existing before StartTransaction
     */
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    /*
     * Attributes of StartTransaction
     */
    char parentIdTag [IDTAG_LEN_MAX + 1] = {'\0'};
    int32_t start_meter = -1;           //meterStart of StartTx
    int transactionId = -1; //only valid if confirmed = true
    /*
     * Attributes of StopTransaction
     */
    char stop_idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    int32_t stop_meter = -1;
    char stop_reason [REASON_LEN_MAX + 1] = {'\0'};
public:
    Transaction(ConnectorTransactionStore& context, unsigned int connectorId, unsigned int txNr, bool silent = false) : 
                ITransaction(context,connectorId,txNr,silent) {}
    /*
     * data assigned by OCPP server
     */
    int getTransactionId() override {return transactionId;}
    /*
     * Getters and setters for (mostly) internal use
     */

    bool setIdTag(const char *idTag) override;
    const char *getIdTag() override {return idTag;}

    bool setParentIdTag(const char *idTag) override;
    const char *getParentIdTag() override {return parentIdTag;}

    void setMeterStart(int32_t meter) override {start_meter = meter;}
    bool isMeterStartDefined() override {return start_meter >= 0;}
    int32_t getMeterStart() override {return start_meter;}

    void setTransactionId(int transactionId) override {this->transactionId = transactionId;}

    bool setStopIdTag(const char *idTag) override;
    const char *getStopIdTag() override {return stop_idTag;}

    void setMeterStop(int32_t meter) override {stop_meter = meter;}
    bool isMeterStopDefined() override {return stop_meter >= 0;}
    int32_t getMeterStop() override {return stop_meter;}

    bool setStopReason(const char *reason) override;
    const char *getStopReason() override {return stop_reason;}
};

} // namespace MicroOcpp

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <memory>

#include <MicroOcpp/Model/Transactions/TransactionDefs.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>

namespace MicroOcpp {
namespace Ocpp201 {

// TriggerReasonEnumType (3.82)
enum class TransactionEventTriggerReason : uint8_t {
    UNDEFINED, // not part of OCPP
    Authorized,
    CablePluggedIn,
    ChargingRateChanged,
    ChargingStateChanged,
    Deauthorized,
    EnergyLimitReached,
    EVCommunicationLost,
    EVConnectTimeout,
    MeterValueClock,
    MeterValuePeriodic,
    TimeLimitReached,
    Trigger,
    UnlockCommand,
    StopAuthorized,
    EVDeparted,
    EVDetected,
    RemoteStop,
    RemoteStart,
    AbnormalCondition,
    SignedDataReceived,
    ResetCommand
};

class Transaction  : public ITransaction{
public:
    // ReasonEnumType (3.67)
    enum class StopReason : uint8_t {
        UNDEFINED, // not part of OCPP
        DeAuthorized,
        EmergencyStop,
        EnergyLimitReached,
        EVDisconnected,
        GroundFault,
        ImmediateReset,
        Local,
        LocalOutOfCredit,
        MasterPass,
        Other,
        OvercurrentFault,
        PowerLoss,
        PowerQuality,
        Reboot,
        Remote,
        SOCLimitReached,
        StoppedByEV,
        TimeLimitReached,
        Timeout
    };
    
    Transaction(ConnectorTransactionStore& context, unsigned int connectorId, unsigned int txNr, bool silent = false) : 
                ITransaction(context,connectorId,txNr,silent) {}

    const char* getTransactionIdStr() override {return transactionId;}
    bool setTransactionIdStr(const char* transactionId) override;

    void sendMeterValue(std::vector<std::unique_ptr<MeterValue>>&& meterValue) override;
    const char *getIdTag() override {return idToken.get()?idToken.get():ITransaction::getIdTag();}
    bool setIdTag(const char *idTag) override {idToken = IdToken(idTag);}
    void setConnectorId(unsigned int connectorId) {ITransaction::setConnectorId(connectorId);notifyEvseId=true;}
    void setAuthorized() override {ITransaction::setAuthorized();notifyIdToken=true;}
    int getRemoteStartId() override {return remoteStartId;}
    void setRemoteStartId(int id) override {remoteStartId=id,notifyRemoteStartId=true;}
    bool setStopIdTag(const char *idTag) override;
    const char *getStopIdTag() override;
// private:
    /*
     * Transaction substates. Notify server about any change when transaction is running
     */
    //bool trackParkingBayOccupancy; // not supported
    bool trackEvConnected;
    bool trackAuthorized;
    bool trackDataSigned;
    bool trackPowerPathClosed;
    bool trackEnergyTransfer;
    /*
     * Global transaction data
     */
    unsigned int seqNoCounter = 0; // increment by 1 for each event
    IdToken idToken;
    char transactionId [MO_TXID_LEN_MAX + 1] = {'\0'};
    int remoteStartId = -1;

    //if to fill next TxEvent with optional fields
    bool notifyEvseId = false;
    bool notifyIdToken = false;
    bool notifyStopIdToken = false;
    bool notifyReservationId = false;
    bool notifyChargingState = false;
    bool notifyRemoteStartId = false;
    bool notifyMeterValue = false;

    bool evConnectionTimeoutListen = true;

    StopReason stopReason = StopReason::UNDEFINED;
    TransactionEventTriggerReason stopTrigger = TransactionEventTriggerReason::UNDEFINED;
    std::unique_ptr<IdToken> stopIdToken; // if null, then stopIdToken equals idToken
    std::vector<std::unique_ptr<MeterValue>> clockMeterValue;
    std::vector<std::unique_ptr<MeterValue>> periodicMeterValue;
    std::vector<std::unique_ptr<MeterValue>> triggerMeterValue;
};

// TransactionEventRequest (1.60.1)
class TransactionEventData {
public:

    // TransactionEventEnumType (3.80)
    enum class Type : uint8_t {
        Ended,
        Started,
        Updated
    };

    // ChargingStateEnumType (3.16)
    enum class ChargingState : uint8_t {
        UNDEFINED, // not part of OCPP
        Charging,
        EVConnected,
        SuspendedEV,
        SuspendedEVSE,
        Idle
    };

//private:
    std::shared_ptr<Transaction> transaction;
    Type eventType;
    Timestamp timestamp;
    TransactionEventTriggerReason triggerReason;
    const unsigned int seqNo;
    bool offline = false;
    int numberOfPhasesUsed = -1;
    int cableMaxCurrent = -1;
    int reservationId = -1;
    int remoteStartId = -1;

    // TransactionType (2.48)
    ChargingState chargingState = ChargingState::UNDEFINED;
    //int timeSpentCharging = 0; // not supported
    std::unique_ptr<IdToken> idToken;
    EvseId evse = -1;
    std::vector<std::unique_ptr<MeterValue>> meterValue;

    TransactionEventData(std::shared_ptr<Transaction> transaction, unsigned int seqNo) : transaction(transaction), seqNo(seqNo) { }
};

} // namespace Ocpp201
} // namespace MicroOcpp

#endif // MO_ENABLE_V201

extern "C" {
#endif //__cplusplus

struct OCPP_Transaction;
typedef struct OCPP_Transaction OCPP_Transaction;

int ocpp_tx_getTransactionId(OCPP_Transaction *tx);
bool ocpp_tx_isAuthorized(OCPP_Transaction *tx);
bool ocpp_tx_isIdTagDeauthorized(OCPP_Transaction *tx);

bool ocpp_tx_isRunning(OCPP_Transaction *tx);
bool ocpp_tx_isActive(OCPP_Transaction *tx);
bool ocpp_tx_isAborted(OCPP_Transaction *tx);
bool ocpp_tx_isCompleted(OCPP_Transaction *tx);

const char *ocpp_tx_getIdTag(OCPP_Transaction *tx);

bool ocpp_tx_getBeginTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

int32_t ocpp_tx_getMeterStart(OCPP_Transaction *tx);

bool ocpp_tx_getStartTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

const char *ocpp_tx_getStopIdTag(OCPP_Transaction *tx);

int32_t ocpp_tx_getMeterStop(OCPP_Transaction *tx);
void ocpp_tx_setMeterStop(OCPP_Transaction* tx, int32_t meter);

bool ocpp_tx_getStopTimestamp(OCPP_Transaction *tx, char *buf, size_t len);

const char *ocpp_tx_getStopReason(OCPP_Transaction *tx);

#ifdef __cplusplus
} //end extern "C"
#endif

#endif
