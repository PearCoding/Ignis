#pragma once

#include "Logger.h"
#include "Target.h"

namespace IG {
enum class SPPMode {
    Fixed,
    Capped,
    Continous
};

enum class ApplicationType {
    View,
    CLI,
    Trace
};


class RuntimeOptions;
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
    std::optional<Vector3f> Eye;
    std::optional<Vector3f> Dir;
    std::optional<Vector3f> Up;

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

    void populate(RuntimeOptions& options) const;
};
} // namespace IG