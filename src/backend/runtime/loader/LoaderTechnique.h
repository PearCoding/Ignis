#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderTechnique {
	static bool requireLights(const LoaderContext& ctx);
	static std::string generate(const LoaderContext& ctx, size_t& aovs);
	static std::string generateHeader(const LoaderContext& ctx, bool isRayGeneration = false);
};
} // namespace IG