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

    std::optional<std::array<float, 3>> Eye;
    std::optional<std::array<float, 3>> Dir;
    std::optional<std::array<float, 3>> Up;

    float Multiplier = 5;
    std::optional<float> VerticalIlluminance;

    inline std::optional<Vector3f> ToVector(const std::optional<std::array<float, 3>>& arr) const
    {
        return arr.has_value() ? std::optional<Vector3f>{ Vector3f(arr.value()[0], arr.value()[1], arr.value()[2]) } : std::optional<Vector3f>{};
    }

    inline std::optional<Vector3f> DirVector() const { return ToVector(Dir); }
    inline std::optional<Vector3f> UpVector() const { return ToVector(Up); }

    IG::Target Target;
    bool NoCache = false;
    Path CacheDir;

    size_t ShaderOptimizationLevel = 3;
    size_t ShaderCompileThreads    = 0;

    Path Input;

    Path ScriptDir;

    void populate(RuntimeOptions& options) const;
};
} // namespace IG