#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct HitShader {
    static std::string setup(int mat_id, LoaderContext& ctx);
};
} // namespace IG