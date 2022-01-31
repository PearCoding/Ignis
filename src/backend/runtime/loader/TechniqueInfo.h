#pragma once

#include "TechniqueVariant.h"

namespace IG {

struct LoaderContext;
using TechniqueCameraGenerator = std::string (*)(const LoaderContext&);

struct TechniqueVariantInfo {
    /// The variant makes uses of ShadowHit and ShadowMiss shaders. Reduces performance
    bool UseAdvancedShadowHandling = false;

    /// The variant makes use of lights
    bool UsesLights = false;

    /// The variant requires all lights (especially area lights) in the miss shader
    bool UsesAllLightsInMiss = false;

    /// The variant requires all materials to be present at all times. Reduces performance significantly [TODO]
    bool RequiresGlobalMaterials = false;

    /// The variant overrides the default camera shader
    TechniqueCameraGenerator OverrideCameraGenerator = nullptr;
};

struct TechniqueInfo {
    /// The AOVs enabled in the current runtime. This option is shared across all variants
    std::vector<std::string> EnabledAOVs;

    /// The variants (or passes) a technique uses. Per default only one variant is available
    std::vector<TechniqueVariantInfo> Variants = { {} };

    /// Callback to select a variant for a specific iteration. If nullptr, variant 0 will be used all the time
    TechniqueVariantSelector VariantSelector = nullptr;
};
} // namespace IG