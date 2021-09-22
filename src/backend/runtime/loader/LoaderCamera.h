#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderCamera {
	static bool load(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG