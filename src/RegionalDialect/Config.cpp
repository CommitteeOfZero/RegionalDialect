
#include <vector>
#include <concepts>
#include <string_view>

#include <log/logger_mgr.hpp>
#include <skyline/utils/cpputils.hpp>

#include "Config.h"

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

JsonWrapper JsonWrapper::operator[](std::string_view item) {
    return JsonWrapper(::cJSON_GetObjectItem(inner, item.data()));
}

template<> int JsonWrapper::get<int>() {
    return static_cast<int>(::cJSON_GetNumberValue(inner));
}

template<> size_t JsonWrapper::get<size_t>() {
    return static_cast<size_t>(::cJSON_GetNumberValue(inner));
}

template<> std::string_view JsonWrapper::get<std::string_view>() {
    char *ret = ::cJSON_GetStringValue(inner);
    return ret ? std::string_view { ret } : std::string_view { };
}

template<> bool JsonWrapper::get<bool>() {
    return static_cast<bool>(::cJSON_IsTrue(inner));
}

template<> std::vector<std::string_view> JsonWrapper::get<std::vector<std::string_view>>() {
    size_t size = (size_t)::cJSON_GetArraySize(inner);
    auto ret = std::vector<std::string_view>();
    ret.reserve(size);

    for (size_t i = 0; i < size; i++)
        ret.emplace_back(JsonWrapper(::cJSON_GetArrayItem(inner, i)).get<std::string_view>());
    return ret;
}

template<> std::vector<JsonWrapper> JsonWrapper::get<std::vector<JsonWrapper>>() {
    size_t size = (size_t)::cJSON_GetArraySize(inner);
    auto ret = std::vector<JsonWrapper>();
    ret.reserve(size);

    for (size_t i = 0; i < size; i++) ret.emplace_back(::cJSON_GetArrayItem(inner, i));

    return ret;
}

template<> float JsonWrapper::get<float>() {
    return static_cast<float>(::cJSON_GetNumberValue(inner));
}

std::string_view JsonWrapper::getName() const {
    return std::string_view { this->inner->string ? this->inner->string : "" };
}

bool JsonWrapper::has(std::string_view item) {
    return ::cJSON_HasObjectItem(inner, item.data()) == 1;
}

void JsonWrapper::print() {
    auto jsonString = ::cJSON_Print(inner);
    Logging.Log("%s\n", jsonString);
    free(jsonString);
}

void Init(std::string const &romMount) {
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
        goto cleanup;
    } else {
        Logging.Log("Successfully parsed gamedef.json\n");
    }

    free((void*)contents);

    rc = skyline::utils::readEntireFile(romMount + "system/patchdef.json", (void**)(&contents), &contentsSize);
    
    if (R_FAILED(rc)) {
        Logging.Log("Failed to load patchdef.json: 0x%x\n", rc);
        goto exit;
    }

    Logging.Log("Successfully loaded patchdef.json: size(%d)\n", contentsSize);

    parseResult = cJSON_ParseWithLength(contents, contentsSize);
    if (parseResult == NULL) {
        Logging.Log("Failed to parse patchdef.json: %s\n", ::cJSON_GetErrorPtr());
        goto exit;
    }

    result = ::cJSON_AddItemToObject(inner, "patchdef", parseResult);
    if (!result || inner == NULL) {
        Logging.Log("Failed to parse patchdef.json: %s\n", ::cJSON_GetErrorPtr());
    } else {
        Logging.Log("Successfully parsed patchdef.json\n");
    }

exit:
    config = JsonWrapper(inner, true);
cleanup:
    free((void*)contents);
}

template int JsonWrapper::get<int>();
template size_t JsonWrapper::get<size_t>();
template std::string_view JsonWrapper::get<std::string_view>();
template std::vector<std::string_view> JsonWrapper::get<std::vector<std::string_view>>();
template bool JsonWrapper::get<bool>();
template float JsonWrapper::get<float>();
template std::vector<JsonWrapper> JsonWrapper::get<std::vector<JsonWrapper>>();

}  // namespace config
}  // namespace rd