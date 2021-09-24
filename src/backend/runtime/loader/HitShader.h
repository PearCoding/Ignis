#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct HitShader {
	static std::string setup(int entity_id, LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG