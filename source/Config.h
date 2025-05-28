#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

#include "cJSON.h"

#ifdef DEFINE_CONFIG
#define CONFIG_GLOBAL
#else
#define CONFIG_GLOBAL extern
#endif

class JsonWrapper {
    private:
        cJSON *inner;
    public:
        JsonWrapper();
        JsonWrapper(cJSON *inner);

        JsonWrapper operator[](const std::string &item);

        template<typename T> T get();
        bool has(const std::string &item);
        void print();
};

CONFIG_GLOBAL JsonWrapper config;

void configInit();

#endif  // !__CONFIG_H__
