#pragma once

#include "LoaderContext.h"

namespace IG {
struct TechniqueInfo {
    std::vector<std::string> EnabledAOVs;
    bool UseAdvancedShadowHandling = false;
};

struct LoaderTechnique {
    static bool requireLights(const LoaderContext& ctx);
    static TechniqueInfo getInfo(const LoaderContext& ctx);
    static std::string generate(const LoaderContext& ctx);
    static std::string generateHeader(const LoaderContext& ctx, bool isRayGeneration = false);
};
} // namespace IG