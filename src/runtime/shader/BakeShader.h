#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct BakeShader {
    /// Will generate function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader
    static std::string end();
};
} // namespace IG