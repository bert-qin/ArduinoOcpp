#include <MicroOcpp/Model/Variables/VariableContainerFlash.h>
#include <MicroOcpp/Model/Variables/Variable.h>
#include <algorithm>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V201

#define MAX_CONFIGURATIONS 100

namespace MicroOcpp
{

    class VariableContainerFlash : public VariableContainer
    {
    private:
        std::vector<std::shared_ptr<Variable>> variables;
        std::shared_ptr<FilesystemAdapter> filesystem;
        uint16_t revisionSum = 0;
        bool loaded = false;
        std::vector<std::unique_ptr<char[]>> keyPool;

        void clearKeyPool(const char *key)
        {
            keyPool.erase(std::remove_if(keyPool.begin(), keyPool.end(),
                                         [key](const std::unique_ptr<char[]> &k)
                                         {
#if MO_DBG_LEVEL >= MO_DL_VERBOSE
                                             if (k.get() == key)
                                             {
                                                 MO_DBG_VERBOSE("clear key %s", key);
                                             }
#endif
                                             return k.get() == key;
                                         }),
                          keyPool.end());
        }

        bool variablesUpdated()
        {
            auto revisionSum_old = revisionSum;

            revisionSum = 0;
            for (auto &var : variables)
            {
                revisionSum += var->getValueRevision();
            }

            return revisionSum != revisionSum_old;
        }

    public:
        VariableContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible) : VariableContainer(filename, accessible), filesystem(filesystem) {}

        // VariableContainer definitions
        bool load() override
        {
            if (loaded)
            {
                return true;
            }

            if (!filesystem)
            {
                return false;
            }

            size_t file_size = 0;
            if (filesystem->stat(getFilename(), &file_size) != 0 // file does not exist
                || file_size == 0)
            { // file exists, but empty
                MO_DBG_DEBUG("Populate FS: create variables file");
                return save();
            }

            auto doc = FilesystemUtils::loadJson(filesystem, getFilename());
            if (!doc)
            {
                MO_DBG_ERR("failed to load %s", getFilename());
                return false;
            }

            JsonObject root = doc->as<JsonObject>();

            JsonObject varHeader = root["head"];

            if (strcmp(varHeader["content-type"] | "Invalid", "ocpp_variable_file"))
            {
                MO_DBG_ERR("Unable to initialize: unrecognized variable file format");
                return false;
            }

            if (strcmp(varHeader["version"] | "Invalid", "1.0"))
            {
                MO_DBG_ERR("Unable to initialize: unsupported version");
                return false;
            }

            JsonArray varsArray = root["variables"];
            if (varsArray.size() > MAX_CONFIGURATIONS)
            {
                MO_DBG_ERR("Unable to initialize: variables_len is too big (=%zu)", varsArray.size());
                return false;
            }

            for (JsonObject stored : varsArray)
            {
                Variable::InternalDataType type;
                if (!deserializeTVar(stored["type"] | "_Undefined", type))
                {
                    MO_DBG_ERR("corrupt config");
                    continue;
                }

                const char *componentName = stored["componentName"] | "";
                if (!*componentName)
                {
                    MO_DBG_ERR("corrupt config");
                    continue;
                }
                const char *name = stored["name"] | "";
                if (!*name)
                {
                    MO_DBG_ERR("corrupt config");
                    continue;
                }

                std::unique_ptr<char[]> componentName_pooled;
                std::unique_ptr<char[]> name_pooled;

                ComponentId componentId(componentName, EvseId(stored["evseId"] | -1, stored["connectorId"] | -1));
                auto var = getVariable(componentId, name);
                if (!var)
                {
                    componentName_pooled.reset(new char[strlen(componentName) + 1]);
                    strcpy(componentName_pooled.get(), componentName);
                    name_pooled.reset(new char[strlen(name) + 1]);
                    strcpy(name_pooled.get(), name);
                    componentId = ComponentId(componentName_pooled.get(), EvseId(stored["evseId"] | -1, stored["connectorId"] | -1));
                    var = createVariable(type, Variable::AttributeTypeSet()); // todo: only actual now
                    var->setComponentId(componentId);
                    var->setName(name_pooled.get());
                }
                if (stored.containsKey("value"))
                {
                    switch (type)
                    {
                    case Variable::InternalDataType::Int:
                    {
                        if (!stored["value"].is<int>())
                        {
                            MO_DBG_ERR("corrupt config");
                            continue;
                        }
                        int value = stored["value"] | 0;
                        var->setInt(value);
                        break;
                    }
                    case Variable::InternalDataType::Bool:
                    {
                        if (!stored["value"].is<bool>())
                        {
                            MO_DBG_ERR("corrupt config");
                            continue;
                        }
                        bool value = stored["value"] | false;
                        var->setBool(value);
                        break;
                    }
                    case Variable::InternalDataType::String:
                    {
                        if (!stored["value"].is<const char *>())
                        {
                            MO_DBG_ERR("corrupt config");
                            continue;
                        }
                        const char *value = stored["value"] | "";
                        var->setString(value);
                        break;
                    }
                    }
                }
                if (componentName_pooled)
                {
                    // allocated key, need to store
                    keyPool.push_back(std::move(componentName_pooled));
                    keyPool.push_back(std::move(name_pooled));
                }
                add(var);
            }

            variablesUpdated();

            MO_DBG_DEBUG("Initialization finished");
            loaded = true;
            return true;
        }

        bool save() override
        {
            if (!filesystem)
            {
                return false;
            }

            if (!variablesUpdated())
            {
                return true; // nothing to be done
            }

            size_t jsonCapacity = 2 * JSON_OBJECT_SIZE(2);           // head + configurations + head payload
            jsonCapacity += JSON_ARRAY_SIZE(variables.size());       // configurations array
            jsonCapacity += variables.size() * JSON_OBJECT_SIZE(11); // config entries in array

            if (jsonCapacity > MO_MAX_JSON_CAPACITY)
            {
                MO_DBG_ERR("variables JSON exceeds maximum capacity (%s, %zu entries). Crop variables file (by FCFS)", getFilename(), variables.size());
                jsonCapacity = MO_MAX_JSON_CAPACITY;
            }

            DynamicJsonDocument doc{jsonCapacity};
            JsonObject head = doc.createNestedObject("head");
            head["content-type"] = "ocpp_variable_file";
            head["version"] = "1.0";

            JsonArray varsArray = doc.createNestedArray("variables");

            size_t trackCapacity = 0;

            for (size_t i = 0; i < variables.size(); i++)
            {
                auto &var = *variables[i];

                size_t entryCapacity = JSON_OBJECT_SIZE(12) + JSON_ARRAY_SIZE(1);
                if (trackCapacity + entryCapacity > MO_MAX_JSON_CAPACITY)
                {
                    break;
                }

                trackCapacity += entryCapacity;

                auto stored = varsArray.createNestedObject();

                stored["type"] = serializeTVar(var.getInternalDataType());
                stored["componentName"] = var.getComponentId().name;
                stored["name"] = var.getName();
                if (var.hasAttribute(Variable::AttributeType::Actual))
                {
                    switch (var.getInternalDataType())
                    {
                    case Variable::InternalDataType::Int:
                        stored["value"] = var.getInt();
                        break;
                    case Variable::InternalDataType::Bool:
                        stored["value"] = var.getBool();
                        break;
                    case Variable::InternalDataType::String:
                        stored["value"] = var.getString();
                        break;
                    }
                }
                // todo componentInstance
                if (var.getComponentId().evse.id > 0)
                {
                    stored["evseId"] = var.getComponentId().evse.id;
                }
                if (var.getComponentId().evse.connectorId > 0)
                {
                    stored["connectorId"] = var.getComponentId().evse.connectorId;
                }
                // todo instance,valTarget,valMin,valMin
            }

            bool success = FilesystemUtils::storeJson(filesystem, getFilename(), doc);

            if (success)
            {
                MO_DBG_DEBUG("Saving variables finished");
            }
            else
            {
                MO_DBG_ERR("could not save variables file: %s", getFilename());
            }

            return success;
        }

        std::shared_ptr<Variable> createVariable(Variable::InternalDataType dtype, Variable::AttributeTypeSet attributes) override
        {
            return makeVariable(dtype, attributes);
        }

        bool add(std::shared_ptr<Variable> variable) override
        {
            variables.push_back(std::move(variable));
            return true;
        }

        size_t size() const override
        {
            return variables.size();
        }

        Variable *getVariable(size_t i) const override
        {
            return variables[i].get();
        }

        std::shared_ptr<Variable> getVariable(const ComponentId &component, const char *variableName) const override
        {
            for (auto it = variables.begin(); it != variables.end(); it++)
            {
                if (!strcmp((*it)->getName(), variableName) &&
                    (*it)->getComponentId().equals(component))
                {
                    return *it;
                }
            }
            return nullptr;
        }

        void loadStaticKey(Variable& var, const ComponentId& component, const char *variableName) override {
            const char* componentName = var.getComponentId().name;
            const char* name = var.getName();
            var.setComponentId(component);
            var.setName(variableName);
            clearKeyPool(componentName);
            clearKeyPool(name);
        }
    };

    std::unique_ptr<VariableContainer> makeVariableContainerFlash(std::shared_ptr<FilesystemAdapter> filesystem, const char *filename, bool accessible)
    {
        return std::unique_ptr<VariableContainer>(new VariableContainerFlash(filesystem, filename, accessible));
    }

} // end namespace MicroOcpp

#endif //MO_ENABLE_V201