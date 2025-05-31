#include <cmath>
#include <cstdarg>

#include "main.hpp"

#include "skyline/logger/TcpLogger.hpp"
#include "skyline/logger/StdoutLogger.hpp"
#include "skyline/utils/ipc.hpp"
#include "skyline/utils/utils.h"

#include "RegionalDialect/Config.h"
#include "RegionalDialect/Text.h"
#include "RegionalDialect/System.h"

// For handling exceptions
char ALIGNA(0x1000) exception_handler_stack[0x4000];
nn::os::UserExceptionInfo exception_info;

std::string RomMountPath;

void exception_handler(nn::os::UserExceptionInfo* info) {
    skyline::logger::s_Instance->LogFormat("Exception occurred!\n");

    skyline::logger::s_Instance->LogFormat("Error description: %x\n", info->ErrorDescription);
    for (int i = 0; i < 29; i++)
        skyline::logger::s_Instance->LogFormat("X[%02i]: %" PRIx64 "\n", i, info->CpuRegisters[i].x);
    skyline::logger::s_Instance->LogFormat("FP: %" PRIx64 "\n", info->FP.x);
    skyline::logger::s_Instance->LogFormat("LR: %" PRIx64 "\n", info->LR.x);
    skyline::logger::s_Instance->LogFormat("SP: %" PRIx64 "\n", info->SP.x);
    skyline::logger::s_Instance->LogFormat("PC: %" PRIx64 "\n", info->PC.x);
    skyline::logger::s_Instance->Flush();
}

uintptr_t codeCaves;

static skyline::utils::Task* after_romfs_task = new skyline::utils::Task{[]() {
    rd::config::Init();
    rd::sys::Init();
    rd::text::Init();
}};

void stub() {}

Result (*nnFsMountRomImpl)(char const*, void*, unsigned long);
Result handleNnFsMountRom(char const* path, void* buffer, unsigned long size) {
    Result rc = 0;
    rc = nnFsMountRomImpl(path, buffer, size);

    if (R_SUCCEEDED(rc))
        skyline::logger::s_Instance->Log("Successfully mounted ROM\n");
    else
        skyline::logger::s_Instance->LogFormat("Failed to mount rom. (0x%x)\n", rc);
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();
    skyline::logger::s_Instance->Flush();

    RomMountPath = std::string(path) + ":/";

    // start task queue
    skyline::utils::SafeTaskQueue* taskQueue = new skyline::utils::SafeTaskQueue(100);
    //taskQueue->startThread(20, 3, 0x4000);
    taskQueue->startThread(20, -2, 0x4000);
    taskQueue->push(new std::unique_ptr<skyline::utils::Task>(after_romfs_task));
    nn::os::WaitEvent(&after_romfs_task->completionEvent);

    return rc;
}

void (*VAbortImpl)(char const*, char const*, char const*, int, Result const*, nn::os::UserExceptionInfo const*, char const*, va_list args);
void handleNnDiagDetailVAbort(char const* str1, char const* str2, char const* str3, int int1, Result const* code, nn::os::UserExceptionInfo const* ExceptionInfo, char const* fmt, va_list args) {
    int len = vsnprintf(nullptr, 0, fmt, args);
    char* fmt_info = new char[len + 1];
    vsprintf(fmt_info, fmt, args);

    const char* fmt_str = "%s\n%s\n%s\n%d\nError: 0x%x\n%s";
    len = snprintf(nullptr, 0, fmt_str, str1, str2, str3, int1, *code, fmt_info);
    char* report = new char[len + 1];
    sprintf(report, fmt_str, str1, str2, str3, int1, *code, fmt_info);

    skyline::logger::s_Instance->LogFormat("%s", report);
    nn::err::ApplicationErrorArg* error =
        new nn::err::ApplicationErrorArg(69, "The software is aborting.", report,
                                         nn::settings::LanguageCode::Make(nn::settings::Language::Language_English));
    nn::err::ShowApplicationError(*error);
    delete[] report;
    delete[] fmt_info;
    VAbortImpl(str1, str2, str3, int1, code, ExceptionInfo, fmt, args);
}

void skyline_main() {
    // populate our own process handle
    Handle h;
    skyline::utils::Ipc::getOwnProcessHandle(&h);
    envSetOwnProcessHandle(h);

    // init hooking setup
    A64HookInit();

    // initialize logger
    skyline::logger::s_Instance = new skyline::logger::StdoutLogger();
    skyline::logger::s_Instance->Log("[skyline_main] Beginning initialization.\n");
    skyline::logger::s_Instance->StartThread();

    // override exception handler to dump info
    nn::os::SetUserExceptionHandler(exception_handler, exception_handler_stack, sizeof(exception_handler_stack),
                                    &exception_info);

    // hook to prevent the game from double mounting romfs
    A64HookFunction(reinterpret_cast<void*>(nn::fs::MountRom), reinterpret_cast<void*>(handleNnFsMountRom),
                    (void**)&nnFsMountRomImpl);

    // manually init nn::ro ourselves, then stub it so the game doesn't try again
    nn::ro::Initialize();
    A64HookFunction(reinterpret_cast<void*>(nn::ro::Initialize), reinterpret_cast<void*>(stub), NULL);

    codeCaves = skyline::utils::g_MainRodataAddr - 0xC30;
}   

extern "C" void skyline_init() {
    skyline::utils::init();
    virtmemSetup();  // needed for libnx JIT

    skyline_main();
}
