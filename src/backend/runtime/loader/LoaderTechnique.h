#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderTechnique {
	static bool requireLights(LoaderContext& ctx);
	static std::string generate(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG