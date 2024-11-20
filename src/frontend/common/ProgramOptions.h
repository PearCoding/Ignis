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
enum class ApplicationType {
    View,
    CLI,
    Trace
};

class ProgramOptions {
public:
    ProgramOptions(int argc, char** argv, ApplicationType type, const std::string& desc);

    ApplicationType Type;

    // If true, program options already handled standard stuff. Just exit the application
    bool ShouldExit = false;

    bool Quiet              = false;
    LogLevel VerbosityLevel = L_INFO;

    bool NoUnused   = false;
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

    IG::Target Target;

    std::optional<size_t> RenderTime; // In seconds
    std::optional<int> SPP;
    std::optional<int> SPI;
    IG::SPPMode SPPMode = SPPMode::Fixed;

    int Seed = 0;

    bool AcquireStats     = false;
    bool AcquireFullStats = false;
    bool DebugTrace       = false;

    bool DumpShader       = false;
    bool DumpFullShader   = false;
    bool DumpRegistry     = false;
    bool DumpFullRegistry = false;

    bool AddExtraEnvLight = false;

    RuntimeOptions::SpecializationMode Specialization = RuntimeOptions::SpecializationMode::Default;

    bool NoStdAOVs = false;
    bool Denoise   = false;

    bool NoCache = false;
    Path CacheDir;

    size_t ShaderOptimizationLevel = 3;
    size_t ShaderCompileThreads    = 0;

    Path Output;
    Path InputScene;
    Path InputRay;

    Path ScriptDir;

    ParameterSet UserEntries;

    std::optional<float> DPI; // Only used for UI

    void populate(RuntimeOptions& options) const;
};
} // namespace IG