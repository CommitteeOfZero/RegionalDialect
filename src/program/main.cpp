#include <cmath>
#include <cstdarg>

#include <log/logger_mgr.hpp>
#include <lib.hpp>
#include <hook/trampoline.hpp>

#include <RegionalDialect/Config.h>
#include <RegionalDialect/System.h>
#include <RegionalDialect/Text.h>

uintptr_t codeCaves;

// void (*VAbortImpl)(char const*, char const*, char const*, int, Result const*, nn::os::UserExceptionInfo const*,
//                    char const*, va_list args);
// void handleNnDiagDetailVAbort(char const* str1, char const* str2, char const* str3, int int1, Result const* code,
//                               nn::os::UserExceptionInfo const* ExceptionInfo, char const* fmt, va_list args) {
//     int len = vsnprintf(nullptr, 0, fmt, args);
//     char* fmt_info = new char[len + 1];
//     vsprintf(fmt_info, fmt, args);

//     const char* fmt_str = "%s\n%s\n%s\n%d\nError: 0x%x\n%s";
//     len = snprintf(nullptr, 0, fmt_str, str1, str2, str3, int1, *code, fmt_info);
//     char* report = new char[len + 1];
//     sprintf(report, fmt_str, str1, str2, str3, int1, *code, fmt_info);

//     Logging.Log("%s", report);
//     nn::err::ApplicationErrorArg* error =
//         new nn::err::ApplicationErrorArg(69, "The software is aborting.", report,
//                                          nn::settings::LanguageCode::Make(nn::settings::Language::Language_English));
//     nn::err::ShowApplicationError(*error);
//     delete[] report;
//     delete[] fmt_info;
//     VAbortImpl(str1, str2, str3, int1, code, ExceptionInfo, fmt, args);
// }

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
