#define DEFINE_CONFIG

#include "Config.h"

#include "skyline/utils/cpputils.hpp"
#include "skyline/logger/StdoutLogger.hpp"

extern std::string RomMountPath;


JsonWrapper::JsonWrapper() {
    inner = cJSON_CreateObject();
}

JsonWrapper::JsonWrapper(cJSON *inner) : inner(inner) {}

JsonWrapper JsonWrapper::operator[](const std::string &item) {
    return JsonWrapper(cJSON_GetObjectItem(inner, item.c_str()));
}

template<> int JsonWrapper::get<int>(const std::string &item) {
    return static_cast<int>(cJSON_GetNumberValue((*this)[item].inner));
}

template<> size_t JsonWrapper::get<size_t>(const std::string &item) {
    return static_cast<size_t>(cJSON_GetNumberValue((*this)[item].inner));
}

template<> char* JsonWrapper::get<char*>(const std::string &item) {
    return cJSON_GetStringValue((*this)[item].inner);
}

bool JsonWrapper::has(const std::string &item) {
    return cJSON_HasObjectItem(inner, item.c_str()) == 1;
}

void JsonWrapper::print() {
    skyline::logger::s_Instance->LogFormat("%s\n", cJSON_Print(inner));
}

void configInit() {
    char *contents;
    Result rc = skyline::utils::readEntireFile(RomMountPath + "/system/gamedef.json", (void**)(&contents), NULL);
    
    if (R_FAILED(rc)) {
        skyline::logger::s_Instance->LogFormat("Failed to load gamedef.json: 0x%x\n", rc);
        return;
    }
    
    skyline::logger::s_Instance->Log("Successfully loaded gamedef.json\n");
    
    cJSON *inner = cJSON_CreateObject();
    cJSON_AddItemToObject(inner, "gamedef", cJSON_Parse(contents));

    config = JsonWrapper(inner);

    free(contents);
}

template int JsonWrapper::get<int>(const std::string &item);
template size_t JsonWrapper::get<size_t>(const std::string &item);
template char *JsonWrapper::get<char*>(const std::string &item);