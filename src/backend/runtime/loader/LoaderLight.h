#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderLight {
	static std::string extract(const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx);
};
} // namespace IG