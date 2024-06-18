// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FIRMWARE_STATUS
#define MO_FIRMWARE_STATUS

namespace MicroOcpp {

enum class FirmwareStatus {
    Downloaded,
    DownloadFailed,
    Downloading,
    Idle,
    InstallationFailed,
    Installing,
    Installed,
# if MO_ENABLE_V201
    DownloadScheduled,
    DownloadPaused,
    InstallRebooting,
    InstallScheduled,
    InstallVerificationFailed,
    InvalidSignature,
    SignatureVerified
#endif    
};

} //end namespace MicroOcpp
#endif
