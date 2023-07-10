#pragma once

#include "device/Target.h"

namespace IG {
struct DenoiserSettings {
    bool Enabled = false; // Enables the denoiser
};

struct GlareOptions {
    bool Enabled = false;
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

    size_t Seed = 0;

    IG::Target Target;
    std::string OverrideTechnique;
    std::string OverrideCamera;
    std::pair<uint32, uint32> OverrideFilmSize = { 0, 0 };

    bool AddExtraEnvLight = false; // User option to add a constant environment light (just to see something)
    Path ScriptDir        = {};    // Path to a new script directory, replacing the internal standard library

    bool EnableCache = true;
    Path CacheDir    = {};

    size_t ShaderOptimizationLevel = 3;

    enum class SpecializationMode {
        Default = 0, // Depending on the parameter it will be embedded or not.
        Force,       // Enforce specialization of generated shader for all parameters. This will increase compile time
        Disable      // Disables specialization of generated shader for all parameters except structural.
    };
    SpecializationMode Specialization = SpecializationMode::Default;

    bool WarnUnused = true; // Warn about unused properties. They might indicate a typo or similar.

    bool DisableStandardAOVs = false; // Disable standard AOVs (e.g., Normal, Albedo)
    DenoiserSettings Denoiser;
    GlareOptions Glare;

    inline static RuntimeOptions makeDefault(bool trace = false)
    {
        auto opts     = RuntimeOptions();
        opts.IsTracer = trace;
        opts.Target   = Target::pickBest();
        return opts;
    }
};
} // namespace IG