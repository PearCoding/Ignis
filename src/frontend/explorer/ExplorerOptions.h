#pragma once

#include "Logger.h"
#include "RuntimeSettings.h"

namespace IG {

struct RuntimeOptions;

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

    bool AcquireStats     = false;
    bool AcquireFullStats = false;
    bool DebugTrace       = false;

    bool DumpShader       = false;
    bool DumpFullShader   = false;
    bool DumpRegistry     = false;
    bool DumpFullRegistry = false;

    RuntimeOptions::SpecializationMode Specialization = RuntimeOptions::SpecializationMode::Default;

    bool NoCache = false;
    Path CacheDir;

    size_t ShaderOptimizationLevel = 3;
    size_t ShaderCompileThreads    = 0;

    void populate(RuntimeOptions& options) const;
};
} // namespace IG