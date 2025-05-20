#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "cJSON.h"

#ifdef DEFINE_CONFIG
#define CONFIG_GLOBAL
#else
#define CONFIG_GLOBAL extern
#endif

CONFIG_GLOBAL cJSON *config;

void configInit();

#endif  // !__CONFIG_H__
