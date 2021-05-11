#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderTexture {
	static bool load(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG