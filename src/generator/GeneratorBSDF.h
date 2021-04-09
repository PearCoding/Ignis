#pragma once

#include "GeneratorContext.h"

namespace IG {
struct BSDFExtractOption {
	std::string SurfaceParameter = "surf";
};
struct GeneratorBSDF {
	static std::string extract(const std::shared_ptr<Loader::Object>& bsdf, const GeneratorContext& ctx, const BSDFExtractOption& options = BSDFExtractOption());
};
} // namespace IG