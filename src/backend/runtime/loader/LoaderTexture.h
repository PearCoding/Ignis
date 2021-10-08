#pragma once

#include "LoaderContext.h"

namespace IG {
class ShadingTree;
struct LoaderTexture {
	static std::string generate(const std::string& name, const Parser::Object& obj, const LoaderContext& ctx, ShadingTree& tree);
};
} // namespace IG