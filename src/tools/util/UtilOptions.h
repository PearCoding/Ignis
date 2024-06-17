#pragma once

#include "Logger.h"

namespace IG {
enum class Operation {
    Convert,
    Info
};

class UtilOptions {
public:
    UtilOptions(int argc, char** argv, const std::string& desc);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;
    bool NoColor            = false;
    bool NoLogo             = false;

    Path InputFile;
    Path OutputFile;

    Operation Op = Operation::Convert;
};
} // namespace IG