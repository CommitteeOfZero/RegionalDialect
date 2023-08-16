#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "json.hpp"
using json = nlohmann::json;

#ifdef DEFINE_CONFIG
#define CONFIG_GLOBAL
#else
#define CONFIG_GLOBAL extern
#endif

CONFIG_GLOBAL json config;
void configInit();


#endif  // !__CONFIG_H__
