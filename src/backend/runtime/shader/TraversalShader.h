#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct TraversalShader {
    /// Will generate technique predefinition, function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader
    static std::string end();
};
} // namespace IG