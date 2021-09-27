#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderLight {
	static std::string generate(LoaderContext& ctx, LoaderResult& result);
};
} // namespace IG