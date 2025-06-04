
#include <vector>

#include "log/logger_mgr.hpp"
#include "skyline/utils/cpputils.hpp"

#include "RegionalDialect/Config.h"

namespace rd {
namespace config {

JsonWrapper::JsonWrapper(JsonWrapper&& other) noexcept : inner(other.inner), takeOwnership(other.takeOwnership) {
    other.inner = nullptr;
    other.takeOwnership = false;
}
JsonWrapper& JsonWrapper::operator=(JsonWrapper&& other) noexcept {
    if (this != &other) {
        if (takeOwnership && inner) {
            ::cJSON_Delete(inner);
        }
        inner = other.inner;
        takeOwnership = other.takeOwnership;
        other.inner = nullptr;
        other.takeOwnership = false;
    }
    return *this;
}

JsonWrapper JsonWrapper::operator[](const std::string &item) {
    return JsonWrapper(::cJSON_GetObjectItem(inner, item.c_str()));
}

template<> int JsonWrapper::get<int>() {
    return static_cast<int>(::cJSON_GetNumberValue(inner));
}

template<> size_t JsonWrapper::get<size_t>() {
    return static_cast<size_t>(::cJSON_GetNumberValue(inner));
}

template<> char* JsonWrapper::get<char*>() {
    return ::cJSON_GetStringValue(inner);
}

template<> std::vector<char*> JsonWrapper::get<std::vector<char*>>() {
    size_t size = (size_t)::cJSON_GetArraySize(inner);
    auto ret = std::vector<char*>();
    for (size_t i = 0; i < size; i++)
        ret.push_back(JsonWrapper(::cJSON_GetArrayItem(inner, i)).get<char*>());
    return ret;
}

bool JsonWrapper::has(const std::string &item) {
    return ::cJSON_HasObjectItem(inner, item.c_str()) == 1;
}

void JsonWrapper::print() {
    auto jsonString = ::cJSON_Print(inner);
    Logging.Log("%s\n", jsonString);
    free(jsonString);
}

void Init(std::string const& romMount) {
    const char *contents;
    size_t contentsSize;
    Result rc = skyline::utils::readEntireFile(romMount + "system/gamedef.json", (void**)(&contents), &contentsSize);
    
    if (R_FAILED(rc)) {
        Logging.Log("Failed to load gamedef.json: 0x%x\n", rc);
        return;
    }
    
    Logging.Log("Successfully loaded gamedef.json: size(%d)\n", contentsSize);
    ::cJSON_InitHooks(nullptr);
    cJSON *inner = ::cJSON_CreateObject();
    cJSON *parseResult = cJSON_ParseWithLength(contents, contentsSize);
    if (parseResult == NULL) {
        Logging.Log("Failed to parse gamedef.json: %s\n", ::cJSON_GetErrorPtr());
        free((void*)contents);
        return;
    }
    bool result = ::cJSON_AddItemToObject(inner, "gamedef", parseResult);
    if (!result || inner == NULL) {
        Logging.Log("Failed to parse gamedef.json: %s\n", ::cJSON_GetErrorPtr());
    } else {
        Logging.Log("Successfully parsed gamedef.json\n");
        config = JsonWrapper(inner, true);
        config.print();     
    }    
    free((void*)contents);
    Logging.Log("Freed contents\n");
}

template int JsonWrapper::get<int>();
template size_t JsonWrapper::get<size_t>();
template char *JsonWrapper::get<char*>();
template std::vector<char*> JsonWrapper::get<std::vector<char*>>();

}  // namespace config
}  // namespace rd