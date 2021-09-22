#pragma once

#include "IG_Config.h"
#include "Target.h"

namespace IG {
class ShaderUtils {
public:
	static std::string constructDevice(Target target);
};
} // namespace IG