#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct RayGenerationShader {
    /// Will generate technique predefinition, function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader. If skipReturn = false, a proper return value will be computed
    static std::string end(const std::string_view emitterName = "emitter", const std::string_view sppName = "spp", bool skipReturn = false);

    /// Will generate the default complete shader. Will call begin & end internally
    static std::string setup(LoaderContext& ctx);
};
} // namespace IG