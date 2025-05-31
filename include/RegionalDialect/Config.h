#pragma once

#ifdef CONFIG_IMPLEMENTATION
#define CONFIG_GLOBAL
#else
#define CONFIG_GLOBAL extern
#endif

#include <string>

#include "cJSON.h"

namespace rd {
namespace config {

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

void Init();

}  // namespace config
}  // namespace rd
