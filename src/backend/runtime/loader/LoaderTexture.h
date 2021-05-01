#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderTexture {
	static std::string extract(const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx);
};
} // namespace IG