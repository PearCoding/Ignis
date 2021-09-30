#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderLight {
	static std::string generate(const LoaderContext& ctx);
};
} // namespace IG