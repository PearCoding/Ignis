#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct HitShader {
    static std::string setup(size_t mat_id, LoaderContext& ctx);
};
} // namespace IG