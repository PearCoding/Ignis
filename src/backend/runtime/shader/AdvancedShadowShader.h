#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct AdvancedShadowShader {
	static std::string setup(bool is_hit, LoaderContext& ctx);
};
} // namespace IG