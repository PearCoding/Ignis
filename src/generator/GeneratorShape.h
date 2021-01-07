#pragma once

#include "GeneratorContext.h"

namespace IG {
struct GeneratorShape {
	static void setup(GeneratorContext& ctx);
	static std::string dump(const GeneratorContext& ctx);
};
} // namespace IG