// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Operations/RequestStartTransaction.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::RequestStartTransaction;

RequestStartTransaction::RequestStartTransaction(TransactionService& txService) : txService(txService) {
  
}

const char* RequestStartTransaction::getOperationType(){
    return "RequestStartTransaction";
}

void RequestStartTransaction::processReq(JsonObject payload) {

    int evseId = payload["evseId"] | 0;
    if (evseId < 0 || evseId >= MO_NUM_EVSE) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    int remoteStartId = payload["remoteStartId"] | 0;
    if (remoteStartId < 0) {
        errorCode = "PropertyConstraintViolation";
        MO_DBG_ERR("IDs must be >= 0");
        return;
    }

    IdToken idToken;
    if (!idToken.parseCstr(payload["idToken"]["idToken"] | (const char*)nullptr, payload["idToken"]["type"] | (const char*)nullptr)) { //parseCstr rejects nullptr as argument
        MO_DBG_ERR("could not parse idToken");
        errorCode = "FormationViolation";
        return;
    }

    IdToken groupIdToken;
        if(payload.containsKey("groupIdToken")){
            if (!groupIdToken.parseCstr(payload["groupIdToken"]["idToken"] | (const char*)nullptr, payload["groupIdToken"]["type"] | (const char*)nullptr)) { //parseCstr rejects nullptr as argument
            MO_DBG_ERR("could not parse groupIdToken");
            errorCode = "FormationViolation";
            return;
        }
    }
    std::unique_ptr<ChargingProfile> chargingProfile;
    auto& model = txService.getContext().getModel();
    int chargingProfileId = -1;
    if (payload.containsKey("chargingProfile") && model.getSmartChargingService()) {
        MO_DBG_INFO("Setting Charging profile via RequestStartTransaction");

        JsonObject chargingProfileJson = payload["chargingProfile"];
        chargingProfile = loadChargingProfile(chargingProfileJson, VER_2_0_1);

        if (!chargingProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "chargingProfile validation failed";
            return;
        }

        if (chargingProfile->getChargingProfilePurpose() != ChargingProfilePurposeType::TxProfile) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "Can only set TxProfile here";
            return;
        }

        if (chargingProfile->getChargingProfileId() < 0) {
            errorCode = "PropertyConstraintViolation";
            errorDescription = "RequestStartTx profile requires non-negative chargingProfileId";
            return;
        }

        chargingProfileId = chargingProfile->getChargingProfileId();
        if(!model.getSmartChargingService()->setChargingProfile(evseId, std::move(chargingProfile))){
            return;
        }
    }

    status = txService.requestStartTransaction(evseId, remoteStartId, idToken, transactionId, groupIdToken, chargingProfileId);
}

std::unique_ptr<DynamicJsonDocument> RequestStartTransaction::createConf(){

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
    JsonObject payload = doc->to<JsonObject>();

    const char *statusCstr = "";

    switch (status) {
        case RequestStartStopStatus_Accepted:
            statusCstr = "Accepted";
            break;
        case RequestStartStopStatus_Rejected:
            statusCstr = "Rejected";
            break;
        default:
            MO_DBG_ERR("internal error");
            break;
    }

    payload["status"] = statusCstr;

    if (*transactionId) {
        payload["transactionId"] = (const char*)transactionId; //force zero-copy mode
    }

    return doc;
}

#endif // MO_ENABLE_V201
