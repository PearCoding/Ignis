#pragma once

#include "Target.h"
#include "table/SceneDatabase.h"

namespace IG {
struct LoaderOptions {
	Target target;
	size_t device;
	bool fusion;
};

struct LoaderResult {
	SceneDatabase Database;
	uint64 DesiredConfiguration;
};

class Loader {
public:
	static bool load(const std::filesystem::path& filepath, const LoaderOptions& opts, LoaderResult& result);
};
} // namespace IG