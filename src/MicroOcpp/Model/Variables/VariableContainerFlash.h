#ifndef MO_VARIABLECONTAINERFLASH_H
#define MO_VARIABLECONTAINERFLASH_H

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Variables/VariableContainer.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {
// variable file format:
// {
//     "head": {
//         "content-type": "ocpp_variable_file",
//         "version": "1.0"
//     },
//     "variables": [
//         {
//             "type": "bool",
//             "componentName": "componentName",
//             "name": "variableName",
//             "value": false,
//             "componentInstance": "",
//             "evseId": -1,
//             "connectorId": -1,
//             "instance": "",
//             "valTarget":null,
//             "valMin":null,
//             "valMax":null
//         }
//     ]
// }
std::unique_ptr<VariableContainer> makeVariableContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible);

} //end namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif