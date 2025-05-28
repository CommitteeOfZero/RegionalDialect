#define DEFINE_CONFIG

#include <vector>

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

template<> int JsonWrapper::get<int>() {
    return static_cast<int>(cJSON_GetNumberValue(inner));
}

template<> size_t JsonWrapper::get<size_t>() {
    return static_cast<size_t>(cJSON_GetNumberValue(inner));
}

template<> char* JsonWrapper::get<char*>() {
    return cJSON_GetStringValue(inner);
}

template<> std::vector<char*> JsonWrapper::get<std::vector<char*>>() {
    size_t size = (size_t)cJSON_GetArraySize(inner);
    auto ret = std::vector<char*>();
    for (size_t i = 0; i < size; i++)
        ret.push_back(JsonWrapper(cJSON_GetArrayItem(inner, i)).get<char*>());
    return ret;
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

template int JsonWrapper::get<int>();
template size_t JsonWrapper::get<size_t>();
template char *JsonWrapper::get<char*>();
template std::vector<char*> JsonWrapper::get<std::vector<char*>>();