#pragma once

#include "device/Target.h"

namespace IG {
struct DenoiserSettings {
    bool Enabled            = false; // Enables the denoiser
    bool FollowSpecular     = false; // Follow specular paths instead of only using the first bounce for AOV information
    bool OnlyFirstIteration = true;  // Acquire AOV information only at the first iteration, or refine every iteration
};

struct RuntimeOptions {
    bool IsTracer          = false;
    bool IsInteractive     = false;
    bool EnableTonemapping = true;
    bool DumpShader        = false;
    bool DumpShaderFull    = false;
    bool DumpRegistry      = false;
    bool DumpRegistryFull  = false;
    bool AcquireStats      = false;
    bool DebugTrace        = false; // Show debug information regarding the calls on the device
    uint32 SPI             = 0;     // Detect automatically
    IG::Target Target;
    std::string OverrideTechnique;
    std::string OverrideCamera;
    std::pair<uint32, uint32> OverrideFilmSize = { 0, 0 };

    bool AddExtraEnvLight           = false; // User option to add a constant environment light (just to see something)
    std::filesystem::path ScriptDir = {};    // Path to a new script directory, replacing the internal standard library

    size_t ShaderOptimizationLevel = 3;

    bool ForceSpecialization = false; // Enforce specialization of generated shader for all parameters. This will increase compile time

    DenoiserSettings Denoiser;

    inline static RuntimeOptions makeDefault(bool trace = false)
    {
        auto opts     = RuntimeOptions();
        opts.IsTracer = trace;
        opts.Target   = Target::pickBest();
        return opts;
    }
};
} // namespace IG