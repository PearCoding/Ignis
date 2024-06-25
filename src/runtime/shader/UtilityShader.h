#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct IG_LIB UtilityShader {
    static std::string setupTonemap(const LoaderContext& ctx);
    static std::string setupGlare(LoaderContext& ctx);
    static std::string setupImageinfo(const LoaderContext& ctx);
};
} // namespace IG