// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/Heartbeat.h>
#include <MicroOcpp/Platform.h>

using namespace MicroOcpp;

HeartbeatService::HeartbeatService(Context& context) : context(context) {
#if MO_ENABLE_V201
    if(context.getVersion().major==2){
        heartbeatIntervalInt = context.getModel().getVariableService()->declareVariable<int>("OCPPCommCtrlr", "HeartbeatInterval", 86400);
    }else
#endif
    {
        heartbeatIntervalInt = declareConfiguration<int>("HeartbeatInterval", 86400);
    }
    lastHeartbeat = mocpp_tick_ms();

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("Heartbeat", [&context] () {
        return new Ocpp16::Heartbeat(context.getModel());});
}

void HeartbeatService::loop() {
    unsigned long hbInterval = heartbeatIntervalInt->getInt();
    hbInterval *= 1000UL; //conversion s -> ms
    unsigned long now = mocpp_tick_ms();

    if (now - lastHeartbeat >= hbInterval) {
        lastHeartbeat = now;

        auto heartbeat = makeRequest(new Ocpp16::Heartbeat(context.getModel()));
        context.initiateRequest(std::move(heartbeat));
    }
}
