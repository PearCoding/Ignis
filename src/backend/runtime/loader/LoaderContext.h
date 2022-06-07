#pragma once

#include "LoaderEnvironment.h"
#include "Target.h"
#include "TechniqueInfo.h"

#include <any>
#include <filesystem>

namespace IG {

struct SceneDatabase;

struct LoaderContext {
    Parser::Scene Scene;

    std::unique_ptr<class LoaderLight> Lights;

    std::filesystem::path FilePath;
    IG::Target Target;
    bool EnablePadding;
    size_t SamplesPerIteration;
    std::unordered_map<std::string, uint32> Images; // Image to Buffer

    std::string CameraType;
    std::string TechniqueType;
    std::string PixelSamplerType;
    IG::TechniqueInfo TechniqueInfo;

    bool IsTracer = false;

    size_t CurrentTechniqueVariant;
    inline const IG::TechniqueVariantInfo CurrentTechniqueVariantInfo() const { return TechniqueInfo.Variants[CurrentTechniqueVariant]; }

    std::unordered_map<std::string, std::any> ExportedData; // Cache with already exported data and auxillary info

    LoaderEnvironment Environment;
    SceneDatabase* Database = nullptr;

    size_t EntityCount;

    // The width & height while loading. This might change in the actual rendering
    size_t FilmWidth  = 800;
    size_t FilmHeight = 600;

    std::filesystem::path handlePath(const std::filesystem::path& path, const Parser::Object& obj) const;

    bool HasError = false;

    /// Use this function to mark the loading process as failed
    inline void signalError()
    {
        HasError = true;
    }
};

} // namespace IG