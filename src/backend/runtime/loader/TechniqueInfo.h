#pragma once

#include "TechniqueVariant.h"

namespace IG {
struct LoaderContext;
using TechniqueCallbackGenerator = std::string (*)(LoaderContext&);
using TechniqueCameraGenerator   = TechniqueCallbackGenerator;

/// Callback returning a list of variants
using TechniqueVariantSelector = std::vector<size_t> (*)(size_t);

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
    std::optional<size_t> OverrideWidth;
    std::optional<size_t> OverrideHeight;

    /// A locked framebuffer will not change and will not count towards the weights
    /// The system is not checking brute force access to the framebuffer!
    /// Using AOVs is still possible
    bool LockFramebuffer = false;

    /// Override the recommended spi
    std::optional<size_t> OverrideSPI;

    inline size_t GetWidth(size_t hint) const
    {
        return OverrideWidth.value_or(hint);
    }

    inline size_t GetHeight(size_t hint) const
    {
        return OverrideHeight.value_or(hint);
    }

    inline size_t GetSPI(size_t hint) const
    {
        return OverrideSPI.value_or(hint);
    }
};

struct TechniqueInfo {
    /// The AOVs enabled in the current runtime. This option is shared across all variants
    std::vector<std::string> EnabledAOVs;

    /// The variants (or passes) a technique uses. Per default only one variant is available
    std::vector<TechniqueVariantInfo> Variants = { {} };

    /// Callback to select the active variants for a specific iteration. If nullptr, all variants will be called sequentially
    TechniqueVariantSelector VariantSelector = nullptr;

    inline size_t ComputeSPI(size_t iter, size_t hintSPI) const
    {
        if (VariantSelector) {
            const auto activeVariants = VariantSelector(iter);
            size_t count              = 0;
            for (const auto& ind : activeVariants) {
                const auto& var = Variants[ind];
                if (!var.LockFramebuffer)
                    count += var.GetSPI(hintSPI);
            }
            return count;
        } else {
            size_t count = 0;
            for (const auto& var : Variants) {
                if (!var.LockFramebuffer)
                    count += var.GetSPI(hintSPI);
            }

            return count;
        }
    }
};
} // namespace IG