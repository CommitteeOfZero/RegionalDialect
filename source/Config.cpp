#define DEFINE_CONFIG

#include "Config.h"

#include "skyline/utils/cpputils.hpp"
#include "skyline/logger/StdoutLogger.hpp"

extern std::string RomMountPath;

void configInit() {
    char *contents;
    Result rc = skyline::utils::readEntireFile(RomMountPath + "/system/gamedef.json", (void**)(&contents), NULL);
    
    if (R_FAILED(rc)) {
        skyline::logger::s_Instance->LogFormat("Failed to load gamedef.json: 0x%x\n", rc);
        return;
    }
    
    skyline::logger::s_Instance->Log("Successfully loaded gamedef.json\n");
    
    config = cJSON_CreateObject();
    cJSON_AddItemToObject(config, "gamedef", cJSON_Parse(contents));

    free(contents);
}
