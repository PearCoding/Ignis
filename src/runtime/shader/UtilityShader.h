#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct UtilityShader {
    static std::string setupTonemap(const LoaderContext& ctx);
    static std::string setupImageinfo(const LoaderContext& ctx);
};
} // namespace IG