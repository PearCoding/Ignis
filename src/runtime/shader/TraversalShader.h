#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct TraversalShader {
    /// Will generate technique predefinition, function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader
    static std::string end();

    static std::string setupPrimary(const LoaderContext& ctx);
    static std::string setupSecondary(const LoaderContext& ctx);

private:
    static std::string setupInternal(const LoaderContext& ctx);
};
} // namespace IG