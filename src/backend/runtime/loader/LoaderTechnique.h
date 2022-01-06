#pragma once

#include "LoaderContext.h"
#include "TechniqueInfo.h"

namespace IG {
struct LoaderTechnique {
    static TechniqueInfo getInfo(const LoaderContext& ctx);
    static std::string generate(const LoaderContext& ctx);
    static std::string generateHeader(const LoaderContext& ctx, bool isRayGeneration = false);
};
} // namespace IG