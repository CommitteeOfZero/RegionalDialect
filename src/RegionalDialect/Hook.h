#pragma once

#include "Config.h"
#include "lib.hpp"
#include "lib/hook/trampoline.hpp"

#define DECLARE_HOOK(name, ret, ...)                                    \
    using name##Func = ret (*)(__VA_ARGS__);                            \
    HOOK_DEFINE_TRAMPOLINE(name) { static ret Callback(__VA_ARGS__); };

#define HOOK_FUNC(category, name)                                            \
    if (rd::config::config["gamedef"]["signatures"][#category].has(#name)) { \
        name::InstallAtPtr(rd::hook::SigScan(#category, #name));             \
    }

namespace rd {
namespace hook {

    uintptr_t FindPattern(const uint8_t* dataStart, const uint8_t* dataEnd, const char* pszPattern,
                          uintptr_t baseAddress, size_t offset, int occurrence);

    uintptr_t SigScan(const char* category, const char* sigName);

    std::vector<uintptr_t> SigScanExhaust(const char* category, const char* sigName);

    std::vector<uintptr_t> SigScanArray(const char* category, const char* sigName, bool exhaust = false);

}  // namespace hook
}  // namespace rd