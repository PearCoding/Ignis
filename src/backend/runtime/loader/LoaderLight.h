#pragma once

#include "LoaderContext.h"

namespace IG {
class ShadingTree;
struct LoaderResult;
struct LoaderLight {
    static void setupAreaLights(LoaderContext& ctx);
    static bool hasAreaLights(const LoaderContext& ctx);
    static std::string generate(ShadingTree& tree, bool skipArea);
    static std::filesystem::path generateLightSelectionCDF(LoaderContext& ctx);
};
} // namespace IG