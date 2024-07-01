// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/SetChargingProfile.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::SetChargingProfile;

SetChargingProfile::SetChargingProfile(Model& model, SmartChargingService& scService) : model(model), scService(scService) {

}

SetChargingProfile::~SetChargingProfile() {

}

const char* SetChargingProfile::getOperationType(){
    return "SetChargingProfile";
}

void SetChargingProfile::processReq(JsonObject payload) {
    const char* idKey = "connectorId";
    const char* profileKey = "csChargingProfiles";
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        idKey = "evseId";
        profileKey = "chargingProfile";
    }
#endif
    int connectorId = payload[idKey] | -1;
    if (connectorId < 0 || !payload.containsKey(profileKey)) {
        errorCode = "FormationViolation";
        return;
    }

    if ((unsigned int) connectorId >= model.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    JsonObject csChargingProfiles = payload[profileKey];

    auto chargingProfile = loadChargingProfile(csChargingProfiles, model.getVersion());
    if (!chargingProfile) {
        errorCode = "PropertyConstraintViolation";
        errorDescription = "csChargingProfiles validation failed";
        return;
    }

    if (chargingProfile->getChargingProfilePurpose() == ChargingProfilePurposeType::TxProfile) {
        // if TxProfile, check if a transaction is running

        if (connectorId == 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set TxProfile at connectorId 0";
            return;
        }

        std::shared_ptr<ITransaction> transaction = nullptr;
#if MO_ENABLE_V201    
        if(model.getVersion().major == 2){
            if(model.getTransactionService() && model.getTransactionService()->getEvse(connectorId))
            transaction = model.getTransactionService()->getEvse(connectorId)->getTransaction();
        }
        else
#endif
        {
            if(model.getConnector(connectorId)){
                transaction = model.getConnector(connectorId)->getTransaction();
            }
        }
        if (!transaction || !transaction->isRunning()) {
            //no transaction running, reject profile
            accepted = false;
            return;
        }
#if MO_ENABLE_V201    
        if(model.getVersion().major == 2){
            if (!chargingProfile->getTransactionIdStr()||strcmp(chargingProfile->getTransactionIdStr(),transaction->getTransactionIdStr())) {
                //transactionId undefined / mismatch
                accepted = false;
                return;
            }
        }else
#endif
        {
            if (chargingProfile->getTransactionId() < 0 ||
                    chargingProfile->getTransactionId() != transaction->getTransactionId()) {
                //transactionId undefined / mismatch
                accepted = false;
                return;
            }

        }

        //seems good
    } else if (chargingProfile->getChargingProfilePurpose() == ChargingProfilePurposeType::ChargePointMaxProfile) {
        if (connectorId > 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Cannot set ChargePointMaxProfile at connectorId > 0";
            return;
        }
    }

    accepted = scService.setChargingProfile(connectorId, std::move(chargingProfile));
}

std::unique_ptr<DynamicJsonDocument> SetChargingProfile::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (accepted) {
        payload["status"] = "Accepted";
    } else {
        payload["status"] = "Rejected";
    }
    return doc;
}
