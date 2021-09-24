#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct MissShader {
	static std::string setup(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG