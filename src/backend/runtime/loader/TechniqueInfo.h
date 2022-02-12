#pragma once

#include "TechniqueVariant.h"

namespace IG {
struct LoaderContext;
using TechniqueCallbackGenerator = std::string (*)(LoaderContext&);
using TechniqueCameraGenerator   = TechniqueCallbackGenerator;

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

    /// Specialized shader generators for special parts of the pipeline
    std::array<TechniqueCallbackGenerator, (size_t)CallbackType::_COUNT> CallbackGenerators{};

    /// Override width & height such that the film width & height is not used
    /// The size of the actual frame buffer stays the same however,
    /// so be careful not to go out of bounds
    int OverrideWidth  = -1;
    int OverrideHeight = -1;

    /// A locked framebuffer will not change and will not count towards the weights
    /// The system is not checking brute force access to the framebuffer!
    /// Using AOVs is still possible
    bool LockFramebuffer = false;
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