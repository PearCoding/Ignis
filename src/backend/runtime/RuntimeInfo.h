#pragma once

#include "IG_Config.h"

namespace IG {
class RuntimeInfo {
public:
	static std::filesystem::path executablePath();
};
} // namespace IG