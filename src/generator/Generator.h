#pragma once

#include "Target.h"

namespace IG {
struct GeneratorOptions {
	Target target;
	size_t device;
	size_t max_path_length;
	size_t spp;
	bool fusion;
	bool force_bvh_build = false;
};

bool generate(const std::filesystem::path& filepath, const GeneratorOptions& opts, std::ostream& os);
} // namespace IG