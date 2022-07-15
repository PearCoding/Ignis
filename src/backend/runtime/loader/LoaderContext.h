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

    bool ForceShadingTreeSpecialization = false;
    ParameterSet LocalRegistry; // Current local registry for given shader
    inline void resetRegistry()
    {
        LocalRegistry = ParameterSet();
    }

    std::unordered_map<std::string, std::any> ExportedData; // Cache with already exported data and auxillary info

    LoaderEnvironment Environment;
    SceneDatabase* Database = nullptr;

    size_t EntityCount;

    // The width & height while loading. This might change in the actual rendering
    size_t FilmWidth  = 800;
    size_t FilmHeight = 600;

    std::filesystem::path handlePath(const std::filesystem::path& path, const Parser::Object& obj) const;

    std::unordered_map<std::string, size_t> RegisteredResources;
    inline size_t registerExternalResource(const std::filesystem::path& path)
    {
        // TODO: Ensure canonical paths?
        auto it = RegisteredResources.find(path.generic_u8string());
        if (it != RegisteredResources.end())
            return it->second;
        const size_t id = RegisteredResources.size();

        return RegisteredResources[path.generic_u8string()] = id;
    }

    inline std::vector<std::string> generateResourceMap() const
    {
        std::vector<std::string> map(RegisteredResources.size());
        for (const auto& p : RegisteredResources) {
            IG_ASSERT(p.second < map.size(), "Access should be always in bound");
            map[p.second] = p.first;
        }
        return map;
    }

    bool HasError = false;

    /// Use this function to mark the loading process as failed
    inline void signalError()
    {
        HasError = true;
    }
};

} // namespace IG