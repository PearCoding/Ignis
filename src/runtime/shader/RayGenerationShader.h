#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct RayGenerationShader {
    /// Will generate technique predefinition, function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader. If skipReturn = false, a proper return value will be computed
    static std::string end(const std::string_view& emitterName = "emitter", const std::string_view& spiName = "spi", bool skipReturn = false);

    /// Will generate the default pixel sampler. Expected to be called between begin & end
    static std::string setupPixelSampler(const LoaderContext& ctx, const std::string_view& varName = "pixel_sampler");

    /// Will generate the default complete shader. Will call begin & end internally
    static std::string setup(LoaderContext& ctx);
};
} // namespace IG