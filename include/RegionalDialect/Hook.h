#pragma once

#include "skyline/inlinehook/And64InlineHook.hpp"

#include "Config.h"

#define DECLARE_HOOK(name, ret, ...)            \
    using name##Func = Result(__VA_ARGS__);     \
    inline name##Func *name##Impl = nullptr;    \
    ret Handle##name(__VA_ARGS__);

#define HOOK_FUNC(category, name)                                               \
    if (rd::config::config["gamedef"]["signatures"][#category].has(#name)) {    \
        A64HookFunction(                                                        \
            reinterpret_cast<void*>(rd::hook::SigScan(#category, #name)),       \
            reinterpret_cast<void*>(Handle##name),                              \
            (void **)&name##Impl                                                \
        );                                                                      \
    }

namespace rd {
namespace hook {

uintptr_t FindPattern(const uint8_t *dataStart, const uint8_t *dataEnd,
                      const char *pszPattern, uintptr_t baseAddress,
                      size_t offset, int occurrence);

uintptr_t SigScan(const char* category, const char* sigName);

std::vector<uintptr_t> SigScanExhaust(const char *category, const char *sigName);

std::vector<uintptr_t> SigScanArray(const char *category, const char *sigName, bool exhaust = false);

}  // namespace hook
}  // namespace rd