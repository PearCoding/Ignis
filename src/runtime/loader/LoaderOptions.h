#pragma once

#include "Parser.h"
#include "RuntimeSettings.h"
#include "Scene.h"

namespace IG {
class ScriptCompiler;
class Device;

struct LoaderOptions {
    std::filesystem::path FilePath;
    IG::Scene Scene;
    IG::Target Target;
    std::string CameraType;
    std::string TechniqueType;
    std::string PixelSamplerType;
    size_t FilmWidth;
    size_t FilmHeight;
    size_t SamplesPerIteration; // Only a recommendation!
    bool IsTracer;
    bool ForceSpecialization;
    bool EnableTonemapping;
    DenoiserSettings Denoiser;

    ScriptCompiler* Compiler;
    IG::Device* Device;
};
} // namespace IG