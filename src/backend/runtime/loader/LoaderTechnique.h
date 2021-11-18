#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderTechnique {
	static size_t getRayStateComponentCount(const LoaderContext& ctx);
	static bool requireLights(const LoaderContext& ctx);
	static std::string generate(const LoaderContext& ctx);
	static std::string generateCameraStateSetter(const LoaderContext& ctx);
};
} // namespace IG