#pragma once

#include "TechniqueVariant.h"

namespace IG {
struct LoaderContext;
using TechniqueCallbackGenerator = std::string (*)(LoaderContext&);
using TechniqueCameraGenerator   = TechniqueCallbackGenerator;

/// Callback returning a list of variants
using TechniqueVariantSelector = std::vector<uint32> (*)(uint32);

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

    /// Override the recommended spi
    int OverrideSPI = -1;

    inline int GetWidth(int hint) const
    {
        return OverrideWidth <= 0 ? hint : OverrideWidth;
    }

    inline int GetHeight(int hint) const
    {
        return OverrideHeight <= 0 ? hint : OverrideHeight;
    }

    inline int GetSPI(int hint) const
    {
        return OverrideSPI <= 0 ? hint : OverrideSPI;
    }
};

struct TechniqueInfo {
    /// The AOVs enabled in the current runtime. This option is shared across all variants
    std::vector<std::string> EnabledAOVs;

    /// The variants (or passes) a technique uses. Per default only one variant is available
    std::vector<TechniqueVariantInfo> Variants = { {} };

    /// Callback to select the active variants for a specific iteration. If nullptr, all variants will be called sequentially
    TechniqueVariantSelector VariantSelector = nullptr;

    inline int ComputeSPI(int iter, int hintSPI) const
    {
        if (VariantSelector) {
            const auto activeVariants = VariantSelector(iter);
            int count                 = 0;
            for (const auto& ind : activeVariants) {
                const auto& var = Variants[ind];
                if (!var.LockFramebuffer)
                    count += var.GetSPI(hintSPI);
            }
            return count;
        } else {
            int count = 0;
            for (const auto& var : Variants) {
                if (!var.LockFramebuffer)
                    count += var.GetSPI(hintSPI);
            }

            return count;
        }
    }
};
} // namespace IG