#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderTexture {
	static bool load(LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG