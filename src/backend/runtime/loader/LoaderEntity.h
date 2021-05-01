#pragma once

#include "LoaderContext.h"

namespace IG {
struct LoaderEntity {
	static void setup(LoaderContext& ctx);
	static std::string dump(const LoaderContext& ctx);
};
} // namespace IG