#pragma once

#include "Parser.h"
#include "RuntimeSettings.h"
#include "Scene.h"

namespace IG {
class ScriptCompiler;
class Device;

struct LoaderOptions {
    Path FilePath;
    Path CachePath;
    const IG::Scene* Scene;
    IG::Target Target;
    std::string CameraType;
    std::string TechniqueType;
    std::string PixelSamplerType;
    size_t FilmWidth;
    size_t FilmHeight;
    size_t SamplesPerIteration; // Only a recommendation!
    bool IsTracer;
    RuntimeOptions::SpecializationMode Specialization;
    bool EnableTonemapping;
    bool EnableCache;
    DenoiserSettings Denoiser;

    ScriptCompiler* Compiler;
    IG::Device* Device;
};
} // namespace IG