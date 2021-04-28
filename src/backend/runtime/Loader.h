#pragma once

#include "Target.h"
#include "serialization/Serializer.h"

namespace IG {
struct LoaderOptions {
	Target target;
	size_t device;
	bool fusion;
};

class Loader {
public:
	static bool load(const std::filesystem::path& filepath, const LoaderOptions& opts, Serializer& stream);
};
} // namespace IG