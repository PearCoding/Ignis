#pragma once

#include "LoaderResult.h"
#include "Parser.h"
#include "RuntimeSettings.h"

namespace IG {
struct LoaderOptions {
    std::filesystem::path FilePath;
    Parser::Scene Scene;
    IG::Target Target;
    std::string CameraType;
    std::string TechniqueType;
    std::string PixelSamplerType;
    size_t FilmWidth;
    size_t FilmHeight;
    size_t SamplesPerIteration; // Only a recommendation!
    bool IsTracer;
    bool ForceSpecialization;
    DenoiserSettings Denoiser;
};

class Loader {
public:
    static bool load(const LoaderOptions& opts, LoaderResult& result);

    /// Get a list of all available techniques
    static std::vector<std::string> getAvailableTechniqueTypes();

    /// Get a list of all available cameras
    static std::vector<std::string> getAvailableCameraTypes();
};
} // namespace IG