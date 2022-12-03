#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct BakeShader {
    /// Will generate function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader
    static std::string end();

    /// Will construct a script baking a texture with the given size.
    static std::string setupTexture2d(const LoaderContext& ctx, const std::string& expr, size_t width, size_t height);
};
} // namespace IG