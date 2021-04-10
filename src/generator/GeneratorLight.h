#pragma once

#include "GeneratorContext.h"

namespace IG {
struct GeneratorLight {
	static std::string extract(const std::string& name, const std::shared_ptr<Loader::Object>& light, const GeneratorContext& ctx);
};
} // namespace IG