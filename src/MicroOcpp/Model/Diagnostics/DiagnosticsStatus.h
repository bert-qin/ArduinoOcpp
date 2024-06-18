// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DIAGNOSTICS_STATUS
#define MO_DIAGNOSTICS_STATUS

namespace MicroOcpp {
enum class DiagnosticsStatus {
    Idle,
    Uploaded,
    UploadFailed,
    Uploading,
#if MO_ENABLE_V201
    BadMessage,
    NotSupportedOperation,
    PermissionDenied,
    AcceptedCanceled
#endif    
};
} //end namespace MicroOcpp
#endif
