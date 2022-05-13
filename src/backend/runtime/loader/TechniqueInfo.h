#pragma once

#include "TechniqueVariant.h"
#include <numeric>

namespace IG {
struct LoaderContext;
using TechniqueCallbackGenerator = std::string (*)(LoaderContext&);
using TechniqueCameraGenerator   = TechniqueCallbackGenerator;

/// Callback returning a list of variants which will be called one after another for the given current iteration
using TechniqueVariantSelector = std::vector<size_t> (*)(size_t /* currentIteration */);

enum class ShadowHandlingMode {
    Simple,               // No advanced shadow handling
    Advanced,             // Advanced shadow handling without specialization. Reduces performance
    AdvancedWithMaterials // Advanced shadow handling with specialization. Reduces performance more
};
struct TechniqueVariantInfo {
    /// The variant shadow handling
    IG::ShadowHandlingMode ShadowHandlingMode = ShadowHandlingMode::Simple;

    /// The variant makes use of lights
    bool UsesLights = false;

    /// The variant makes use of participated media
    bool UsesMedia = false;

    /// The variant requires all lights (especially area lights) in the miss shader, else only infinite lights will be exposed in the miss shader
    bool UsesAllLightsInMiss = false;

    /// The variant overrides the default camera shader
    TechniqueCameraGenerator OverrideCameraGenerator = nullptr;

    /// The variant requires the camera definition in the miss, hit and advanced shadow shaders
    bool RequiresExplicitCamera = false;

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

    [[nodiscard]] inline size_t ComputeSPI(size_t iter, size_t hintSPI) const
    {
        if (VariantSelector) {
            const auto activeVariants = VariantSelector(iter);
            IG_ASSERT(activeVariants.size() > 0, "Expected some variants to be returned by the technique variant selector");

            return std::accumulate(activeVariants.begin(), activeVariants.end(), size_t(0),
                                   [&](size_t cur, size_t ind) {
                                       const auto& var = Variants[ind];
                                       return !var.LockFramebuffer ? (cur + var.GetSPI(hintSPI)) : cur;
                                   });
        } else {
            return std::accumulate(Variants.begin(), Variants.end(), size_t(0),
                                   [&](size_t cur, const TechniqueVariantInfo& var) {
                                       return !var.LockFramebuffer ? (cur + var.GetSPI(hintSPI)) : cur;
                                   });
        }
    }
};
} // namespace IG