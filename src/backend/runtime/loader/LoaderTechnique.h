#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderResult;
struct LoaderTechnique {
	static std::string generate(bool is_miss_shader, LoaderContext& ctx, LoaderResult& res);
};
} // namespace IG