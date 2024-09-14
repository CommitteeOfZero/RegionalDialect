#include "skyline/logger/StdoutLogger.hpp"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "skyline/nx/kernel/svc.h"

#ifdef __cplusplus
}
#endif

namespace skyline::logger {

StdoutLogger::StdoutLogger() {}

void StdoutLogger::Initialize() {
    // nothing to do
}

void StdoutLogger::SendRaw(void* data, size_t size) {
    const char* str = (const char*)data;
    printf("%s", str);
};
};  // namespace skyline::logger