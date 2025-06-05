#include <cmath>
#include <cstdarg>

#include <log/logger_mgr.hpp>
#include <lib.hpp>
#include <hook/trampoline.hpp>

#include "RegionalDialect/Config.h"
#include "RegionalDialect/System.h"
#include "RegionalDialect/Text.h"
#include "RegionalDialect/Vm.h"

uintptr_t codeCaves = 0;

namespace nn {
namespace fs {
    Result MountRom(char const* path, void* buffer, unsigned long size);
}  // namespace fs
}  // namespace nn

// clang-format off
HOOK_DEFINE_TRAMPOLINE(MountRom) {
    static Result Callback(char const* path, void* buffer, unsigned long size) {
        static bool hasMounted = false;
        Logging.Log("[RegionalDialect] Mounting ROM\n");
        Result ret{};
        if (!hasMounted) {
            ret = Orig(path, buffer, size);
            if (R_SUCCEEDED(ret)) {
                Logging.Log("[RegionalDialect] Mounted ROM successfully.\n");
                hasMounted = true;
                std::string romMount = std::string(path) + ":/"; 
                rd::config::Init(romMount);
                Logging.Log("[RegionalDialect] Finished config init.\n");
                rd::sys::Init();
                Logging.Log("[RegionalDialect] Finished sys init.\n");
                rd::vm::Init();
                Logging.Log("[RegionalDialect] Finished vm init.\n");
                rd::text::Init(romMount);
                Logging.Log("[RegionalDialect] Finished initialization.\n");
            } else {
                Logging.Log("[RegionalDialect] Failed to mount ROM: 0x%x\n", ret);
            }
        }       
        return ret;
    }
};
// clang-format on

extern "C" void exl_main(void* x0, void* x1) {
    /* Setup hooking environment. */
    exl::hook::Initialize();

    Logging.Log("[RegionalDialect] Beginning initialization.\n");

    codeCaves = exl::util::GetMainModuleInfo().m_Rodata.m_Start - 0xC30;
    MountRom::InstallAtFuncPtr(nn::fs::MountRom);
}

extern "C" NORETURN void exl_exception_entry() { EXL_ABORT("Default exception handler called!"); }
