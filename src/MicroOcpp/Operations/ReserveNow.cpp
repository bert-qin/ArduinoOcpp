// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_RESERVATION

#include <MicroOcpp/Operations/ReserveNow.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Model/Variables/VariableService.h>

using MicroOcpp::Ocpp16::ReserveNow;

ReserveNow::ReserveNow(Model& model) : model(model) {
  
}

ReserveNow::~ReserveNow() {
  
}

const char* ReserveNow::getOperationType(){
    return "ReserveNow";
}

void ReserveNow::processReq(JsonObject payload) {
    const char* connectorIdKey = "connectorId";
    const char* expiryDateKey = "expiryDate";
    const char* idTagKey = "idTag";
    const char* reserveIdKey = "reservationId";
    int connectorId = -1;
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        connectorIdKey = "evseId";
        expiryDateKey = "expiryDateTime";
        idTagKey = "idToken";
        reserveIdKey = "id";
        if (payload[connectorIdKey]|0 < 0) {
            errorCode = "FormationViolation";
            return;
        }
        connectorId = payload[connectorIdKey] | 0;
    }else
#endif
    {
        if (!payload.containsKey(connectorIdKey) ||
                payload[connectorIdKey] < 0) {
            errorCode = "FormationViolation";
            return;
        }
        connectorId = payload[connectorIdKey] | -1;
    }

    if (!payload.containsKey(expiryDateKey) ||
            !payload.containsKey(idTagKey) ||
            //parentIdTag is optional
            !payload.containsKey(reserveIdKey)) {
        errorCode = "FormationViolation";
        return;
    }

    if (connectorId < 0 || (unsigned int) connectorId >= model.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    Timestamp expiryDate;
    if (!expiryDate.setTime(payload[expiryDateKey])) {
        MO_DBG_WARN("bad time format");
        errorCode = "PropertyConstraintViolation";
        return;
    }

const char *idTag = "";
#if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        idTag = payload[idTagKey][idTagKey] | "";
    }else
#endif
    {
        idTag = payload[idTagKey] | "";
    }
    if (!*idTag) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    const char *parentIdTag = nullptr;
    #if MO_ENABLE_V201
    if(model.getVersion().major == 2){
        if (payload.containsKey("groupIdToken")) {
            parentIdTag = payload["groupIdToken"][idTagKey];
        }
    }else
#endif
    {
        if (payload.containsKey("parentIdTag")) {
            parentIdTag = payload["parentIdTag"];
        }
    }

    int reservationId = payload[reserveIdKey] | -1;

    if (model.getReservationService() &&
                model.getNumConnectors() > 0) {
        auto rService = model.getReservationService();
        auto chargePoint = model.getConnector(0);

        std::shared_ptr<ICfg> reserveConnectorZeroSupportedBool;
#if MO_ENABLE_V201
        if(model.getVersion().major == 2){
            auto varService = model.getVariableService();
            reserveConnectorZeroSupportedBool = varService->declareVariable<bool>("ReservationCtrlr", "NonEvseSpecific", true, MO_VARIABLE_VOLATILE, Variable::Mutability::ReadOnly);
        }else
#endif
        {
            reserveConnectorZeroSupportedBool  = declareConfiguration<bool>("ReserveConnectorZeroSupported", true, CONFIGURATION_VOLATILE);
        }

        if (connectorId == 0 && (!reserveConnectorZeroSupportedBool || !reserveConnectorZeroSupportedBool->getBool())) {
            reservationStatus = "Rejected";
            return;
        }

        if (auto reservation = rService->getReservationById(reservationId)) {
            reservation->update(reservationId, (unsigned int) connectorId, expiryDate, idTag, parentIdTag);
            reservationStatus = "Accepted";
            return;
        }

        Connector *connector = nullptr;
        
        if (connectorId > 0) {
            connector = model.getConnector((unsigned int) connectorId);
        }

        if (chargePoint->getStatus() == ChargePointStatus_Faulted ||
                (connector && connector->getStatus() == ChargePointStatus_Faulted)) {
            reservationStatus = "Faulted";
            return;
        }

        if (chargePoint->getStatus() == ChargePointStatus_Unavailable ||
                (connector && connector->getStatus() == ChargePointStatus_Unavailable)) {
            reservationStatus = "Unavailable";
            return;
        }

        if (connector && connector->getStatus() != ChargePointStatus_Available) {
            reservationStatus = "Occupied";
            return;
        }

        if (rService->updateReservation(reservationId, (unsigned int) connectorId, expiryDate, idTag, parentIdTag)) {
            reservationStatus = "Accepted";
        } else {
            reservationStatus = "Occupied";
        }
    } else {
        errorCode = "InternalError";
    }
}

std::unique_ptr<DynamicJsonDocument> ReserveNow::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();

    if (reservationStatus) {
        payload["status"] = reservationStatus;
    } else {
        MO_DBG_ERR("didn't set reservationStatus");
        payload["status"] = "Rejected";
    }
    
    return doc;
}

#endif //MO_ENABLE_RESERVATION
