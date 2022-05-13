#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct AdvancedShadowShader {
    static std::string setup(bool is_hit, size_t mat_id, LoaderContext& ctx);
};
} // namespace IG