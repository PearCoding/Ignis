#pragma once

#include "LoaderContext.h"

namespace IG {
struct AOVInfo {
	std::vector<std::string> EnabledAOVs;
};

struct LoaderTechnique {
	static bool requireLights(const LoaderContext& ctx);
	static AOVInfo getAOVInfo(const LoaderContext& ctx);
	static std::string generate(const LoaderContext& ctx);
	static std::string generateHeader(const LoaderContext& ctx, bool isRayGeneration = false);
};
} // namespace IG