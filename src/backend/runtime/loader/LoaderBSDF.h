#pragma once

#include "LoaderContext.h"

namespace IG {
struct BSDFExtractOption {
	std::string SurfaceParameter = "surf";
};
struct LoaderBSDF {
	static std::string prepare(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx);
	static std::string extract(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx, const BSDFExtractOption& options = BSDFExtractOption());
};
} // namespace IG