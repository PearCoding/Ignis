#pragma once

#include "loader/LoaderContext.h"

namespace IG {
class ShaderBuilder;

struct HitShader {
    static ShaderBuilder setup(size_t mat_id, LoaderContext& ctx);
};
} // namespace IG