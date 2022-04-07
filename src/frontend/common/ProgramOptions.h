#pragma once

#include "Logger.h"
#include "SPPMode.h"
#include "Target.h"

#include <array>
#include <optional>

namespace IG {
enum class ApplicationType {
    View,
    CLI,
    Trace
};

struct RuntimeOptions;
class ProgramOptions {
public:
    ProgramOptions(int argc, char** argv, ApplicationType type, const std::string& desc);

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;

    bool NoColor    = false;
    bool NoProgress = false;

    std::optional<int> Width;
    std::optional<int> Height;
    std::optional<std::array<float, 3>> Eye;
    std::optional<std::array<float, 3>> Dir;
    std::optional<std::array<float, 3>> Up;

    inline std::optional<Vector3f> ToVector(const std::optional<std::array<float, 3>>& arr) const
    {
        return arr.has_value() ? std::optional<Vector3f>{ Vector3f(arr.value()[0], arr.value()[1], arr.value()[2]) } : std::optional<Vector3f>{};
    }

    inline std::optional<Vector3f> EyeVector() const { return ToVector(Eye); }
    inline std::optional<Vector3f> DirVector() const { return ToVector(Dir); }
    inline std::optional<Vector3f> UpVector() const { return ToVector(Up); }

    std::string CameraType;
    std::string TechniqueType;

    IG::Target Target  = Target::INVALID;
    int Device         = 0;
    bool AutodetectCPU = false;
    bool AutodetectGPU = false;

    std::optional<int> SPP;
    std::optional<int> SPI;
    IG::SPPMode SPPMode = SPPMode::Fixed;

    bool AcquireStats     = false;
    bool AcquireFullStats = false;

    bool DumpShader     = false;
    bool DumpFullShader = false;

    std::filesystem::path Output;
    std::filesystem::path InputScene;
    std::filesystem::path InputRay;

    std::filesystem::path ScriptDir;

    void populate(RuntimeOptions& options) const;
};
} // namespace IG