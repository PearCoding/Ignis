#pragma once

#include "TechniqueDescriptor.h"
#include <functional>
#include <numeric>

namespace IG {
class LoaderContext;
using TechniqueCallbackGenerator = std::function<std::string(LoaderContext&)>;
using TechniqueCameraGenerator   = TechniqueCallbackGenerator;

/// Callback returning a list of variants which will be called one after another for the given current iteration
using TechniquePassSelector = std::function<std::vector<size_t>(size_t /* currentIteration */)>;

enum class ShadowHandlingMode {
    Simple                = 0, // No advanced shadow handling, given color will be splat directly if ray 'misses'.
    Advanced              = 1, // Advanced shadow handling without specialization. Reduces performance
    AdvancedWithMaterials = 2  // Advanced shadow handling with specialization. Reduces performance more
};
struct TechniquePassInfo {
    /// The variant shadow handling
    IG::ShadowHandlingMode ShadowHandlingMode = ShadowHandlingMode::Simple;

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

    [[nodiscard]] inline size_t GetWidth(size_t hint) const
    {
        return std::max<size_t>(1, OverrideWidth.value_or(hint));
    }

    [[nodiscard]] inline size_t GetHeight(size_t hint) const
    {
        return std::max<size_t>(1, OverrideHeight.value_or(hint));
    }

    [[nodiscard]] inline size_t GetSPI(size_t hint) const
    {
        return std::max<size_t>(1, OverrideSPI.value_or(hint));
    }
};

struct TechniqueInfo {
    /// The variants (or passes) a technique uses. Per default only one variant is available
    std::vector<TechniquePassInfo> Passes = { {} };

    /// Callback to select the active variants for a specific iteration. If nullptr, all variants will be called sequentially
    TechniquePassSelector PassSelector = nullptr;

    /// The technique makes use of lights
    bool UsesLights = false;

    /// The technique makes use of participated media
    bool UsesMedia = false;

    /// The technique requires all lights (especially area lights) in the miss shader, else only infinite lights will be exposed in the miss shader
    bool UsesAllLightsInMiss = false;

    /// Number of entries of the primary ray payload (on_hit, on_shadow, on_miss, on_bounce)
    size_t PrimaryPayloadCount = 0;

    /// Number of entries of the secondary ray payload has (on_shadow, on_shadow_miss, on_shadow_hit)
    size_t SecondaryPayloadCount = 0;

    /// Name of the EmitterPayloadInitializer generator. If not set, 'make_null_emitter_payload_initializer' will be used. Will be ignored if default camera generator is overriden
    std::string EmitterPayloadInitializer = {};

    /// The technique overrides the default camera shader
    TechniqueCameraGenerator OverrideCameraGenerator = nullptr;

    /// The technique requires the camera definition in the miss, hit and advanced shadow shaders
    bool RequiresExplicitCamera = false;

    /// Specialized shader generators for special parts of the pipeline
    std::array<TechniqueCallbackGenerator, (size_t)CallbackType::_COUNT> CallbackGenerators{};

    [[nodiscard]] inline size_t ComputeSPI(size_t iter, size_t hintSPI) const
    {
        if (PassSelector) {
            const auto activePass = PassSelector(iter);
            IG_ASSERT(activePass.size() > 0, "Expected some variants to be returned by the technique variant selector");

            return std::accumulate(activePass.begin(), activePass.end(), size_t(0),
                                   [&](size_t cur, size_t ind) {
                                       const auto& var = Passes[ind];
                                       return !var.LockFramebuffer ? (cur + var.GetSPI(hintSPI)) : cur;
                                   });
        } else {
            return std::accumulate(Passes.begin(), Passes.end(), size_t(0),
                                   [&](size_t cur, const TechniquePassInfo& var) {
                                       return !var.LockFramebuffer ? (cur + var.GetSPI(hintSPI)) : cur;
                                   });
        }
    }

    [[nodiscard]] inline std::string getEmitterPayloadInitializer() const
    {
        if (EmitterPayloadInitializer.empty())
            return "empty_payload_initializer";
        else
            return EmitterPayloadInitializer;
    }

    [[nodiscard]] inline ShadowHandlingMode getMaximumShadowHandlingMode() const
    {
        ShadowHandlingMode maxShadowMode = ShadowHandlingMode::Simple;
        for (const auto& pass : Passes)
            maxShadowMode = (ShadowHandlingMode)std::max((int)maxShadowMode, (int)pass.ShadowHandlingMode);
        return maxShadowMode;
    }
};
} // namespace IG