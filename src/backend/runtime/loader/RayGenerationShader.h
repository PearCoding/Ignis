#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct RayGenerationShader {
	static std::string setup(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG