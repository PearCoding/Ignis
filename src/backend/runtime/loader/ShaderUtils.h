#pragma once

#include "LoaderContext.h"
#include "Target.h"

namespace IG {
class ShaderUtils {
public:
	static std::string constructDevice(Target target);
	static std::string generateSceneInfoInline(const LoaderContext& ctx);

	static std::string inlineVector(const Vector3f& pos);
	static std::string inlineColor(const Vector3f& color);
};
} // namespace IG