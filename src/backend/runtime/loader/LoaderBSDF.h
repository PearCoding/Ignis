#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderBSDF {
	static bool load(LoaderContext& ctx, LoaderResult& result);
};
} // namespace IG