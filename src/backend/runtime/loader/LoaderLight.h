#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderLight {
	static void setupAreaLights(LoaderContext& ctx);
	static bool hasAreaLights(const LoaderContext& ctx);
	static std::string generate(const LoaderContext& ctx, bool skipArea);
};
} // namespace IG