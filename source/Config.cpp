#define DEFINE_CONFIG

#include <sstream>
#include "Config.h"

#include "skyline/utils/cpputils.hpp"
#include "skyline/logger/StdoutLogger.hpp"

extern std::string RomMountPath;

void configLoadFiles();

void configInit() {
  configLoadFiles();
}


void configLoadFiles() {
  char *contents;

  R_ERRORONFAIL(skyline::utils::readEntireFile(RomMountPath + "/system/gamedef.json", reinterpret_cast<void**>(&contents), NULL));
  
  std::stringstream output(contents);

  json j;
  output >> j;
  config["gamedef"] = j;
}