#pragma once

#include "Logger.h"
#include "ParameterSet.h"
#include "RuntimeSettings.h"
#include "RuntimeStructs.h"
#include "SPPMode.h"
#include "device/Target.h"

#include <array>
#include <optional>

namespace IG {
class GlareOptions {
public:
    GlareOptions(int argc, char** argv);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;

    bool NoUnused   = false;
    bool NoColor    = false;
    bool NoProgress = false;

    float Multiplier = 5;
    std::optional<float> VerticalIlluminance;

    IG::Target Target;
    bool NoCache = false;
    Path CacheDir;

    size_t ShaderOptimizationLevel = 3;
    size_t ShaderCompileThreads    = 0;

    Path Input;
    std::string LayerName;

    Path ScriptDir;

    void populate(RuntimeOptions& options) const;
};
} // namespace IG