#pragma once

#include <string>

#include "lib/log/logger_mgr.hpp"
#include <cJSON/cJSON.h>

namespace rd {
namespace config {

    class JsonWrapper {
       private:
        cJSON* inner{};
        bool takeOwnership = false;

       public:
        JsonWrapper() = default;
        JsonWrapper(cJSON* inner, bool takeOwnership = false) : inner(inner), takeOwnership(takeOwnership) {}
        JsonWrapper(JsonWrapper const&) = delete;             // Disable copy constructor
        JsonWrapper& operator=(JsonWrapper const&) = delete;  // Disable copy assignment
        JsonWrapper(JsonWrapper&& other) noexcept;
        JsonWrapper& operator=(JsonWrapper&& other) noexcept;
        ~JsonWrapper() {
            if (takeOwnership && inner) {
                ::cJSON_Delete(inner);
                Logging.Log("JsonWrapper deleted cJSON object.\n");
            }
        }
        cJSON const* raw() const { return inner; }
        JsonWrapper operator[](const std::string& item);

        template <typename T>
        T get();
        bool has(const std::string& item);
        void print();
    };

    inline JsonWrapper config;

    void Init(std::string const& romMount);

}  // namespace config
}  // namespace rd
