#pragma once

#include "IG_Config.h"

namespace IG {
class ImageIO {
public:
	static bool save(const std::filesystem::path& path, size_t width, size_t height,
					 const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names);
};
} // namespace IG
