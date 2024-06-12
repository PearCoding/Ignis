#pragma once

#include "Logger.h"

namespace IG {
class CompilerOptions {
public:
    CompilerOptions(int argc, char** argv, const std::string& desc);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;
    bool NoColor            = false;

    size_t OptimizationLevel = 3;

    Path InputFile;
};
} // namespace IG