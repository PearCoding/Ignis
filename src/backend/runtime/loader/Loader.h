#pragma once

#include "Parser.h"
#include "Target.h"
#include "TechniqueInfo.h"
#include "table/SceneDatabase.h"

namespace IG {
constexpr size_t DefaultAlignment = sizeof(float) * 4;

struct LoaderOptions {
    std::filesystem::path FilePath;
    Parser::Scene Scene;
    IG::Target Target;
    std::string CameraType;
    std::string TechniqueType;
    size_t SamplesPerIteration; // Only a recommendation!
};

struct LoaderResult {
    SceneDatabase Database;
    std::vector<TechniqueVariant> TechniqueVariants;
    IG::TechniqueInfo TechniqueInfo;
};

class Loader {
public:
    static bool load(const LoaderOptions& opts, LoaderResult& result);
};
} // namespace IG