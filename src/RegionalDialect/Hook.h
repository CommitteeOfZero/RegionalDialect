#pragma once

#include <lib.hpp>
#include <lib/hook/trampoline.hpp>

#include "Config.h"

#define DECLARE_HOOK(name, ret, ...)                                                    \
    HOOK_DEFINE_TRAMPOLINE(name) { static ret Callback(__VA_ARGS__); };

#define HOOK_FUNC(category, name)                                                       \
    [&]{                                                                                \
        if (!rd::config::config["gamedef"]["signatures"][#category].has(#name)) return; \
        name::InstallAtPtr(rd::hook::SigScan(#category, #name));                        \
    }()

#define HOOK_VAR(category, name)                                                        \
    name = reinterpret_cast<decltype(name)>(rd::hook::SigScan(#category, #name));

namespace rd {
namespace hook {

    uintptr_t FindPattern(const uint8_t* dataStart, const uint8_t* dataEnd, const char* pszPattern,
                          uintptr_t baseAddress, size_t offset, int occurrence);

    uintptr_t SigScan(const char* category, const char* sigName);

    std::vector<uintptr_t> SigScanExhaust(const char* category, const char* sigName);

    std::vector<uintptr_t> SigScanArray(const char* category, const char* sigName, bool exhaust = false);

}  // namespace hook
}  // namespace rd