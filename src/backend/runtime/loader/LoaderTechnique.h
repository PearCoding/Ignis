#pragma once

#include "LoaderContext.h"
#include "TechniqueVariant.h"

namespace IG {
struct TechniqueInfo {
    std::vector<std::string> EnabledAOVs;
    bool UseAdvancedShadowHandling           = false;
    uint32 VariantCount                      = 1;
    TechniqueVariantSelector VariantSelector = nullptr;
};

struct LoaderTechnique {
    static bool requireLights(const LoaderContext& ctx);
    static TechniqueInfo getInfo(const LoaderContext& ctx);
    static std::string generate(const LoaderContext& ctx);
    static std::string generateHeader(const LoaderContext& ctx, bool isRayGeneration = false);
};
} // namespace IG