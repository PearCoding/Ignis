#pragma once

#include "TechniqueVariant.h"

namespace IG {

struct LoaderContext;
using TechniqueCameraGenerator = std::string (*)(const LoaderContext&);
struct TechniqueInfo {
    /// The AOVs enabled in the current runtime. This option is shared across all variants
    std::vector<std::string> EnabledAOVs;

    /// The technique makes uses of ShadowHit and ShadowMiss shaders. Reduces performances. This option is per variant
    std::vector<bool> UseAdvancedShadowHandling = { false };

    /// The technique makes use of lights. This option is per variant
    std::vector<bool> UsesLights = { false };

    /// The technique requires all lights (especially area lights) in the miss shader. This option is per variant
    std::vector<bool> UsesAllLightsInMiss = { false };

    /// The technique requires all materials present at all times. Reduces performances significantly. This option is per variant
    std::vector<bool> RequiresGlobalMaterials = { false };

    /// The technique overrides the default camera shader. This option is per variant
    std::vector<TechniqueCameraGenerator> OverrideCameraGenerator = { nullptr };

    /// The number of variants the technique uses
    uint32 VariantCount = 1;

    /// Callback to select a variant for a specific iteration. If nullptr, variant 0 will be used all the time
    TechniqueVariantSelector VariantSelector = nullptr;
};
} // namespace IG