#pragma once

#include "Logger.h"

namespace IG {
class ExplorerOptions {
public:
    ExplorerOptions(int argc, char** argv, const std::string& desc);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;
    bool NoColor            = false;

    Path InputFile;

    size_t WindowWidth  = 1280;
    size_t WindowHeight = 720;

    std::optional<float> DPI; // Only used for UI
};
} // namespace IG