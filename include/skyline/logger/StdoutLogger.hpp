#pragma once

#include <string>

#include "skyline/logger/Logger.hpp"

namespace skyline::logger {
class StdoutLogger : public Logger {
   public:
    StdoutLogger();

    virtual void Initialize();
    virtual void SendRaw(void*, size_t);
    virtual std::string FriendlyName() { return "StdoutLogger"; }
};
};  // namespace skyline::logger
