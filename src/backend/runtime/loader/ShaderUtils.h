#pragma once

#include "LoaderContext.h"
#include "Target.h"

namespace IG {
class ShaderUtils {
public:
	static std::string constructDevice(Target target);
	static std::string generateSceneInfoInline(const LoaderContext& ctx);
};
} // namespace IG