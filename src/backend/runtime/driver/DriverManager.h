#pragma once

#include "Interface.h"
#include "SharedLibrary.h"
#include "Target.h"

#include <unordered_map>
#include <unordered_set>

namespace IG {
class DriverManager {
public:
	bool init(const std::filesystem::path& dir = std::filesystem::current_path(), bool ignoreEnv = false);
	uint64 checkConfiguration(uint64 config) const;
	bool load(uint64 config, DriverInterface& interface) const;

private:
	bool addModule(const std::filesystem::path& path);
	std::unordered_map<uint64, SharedLibrary> mLoadedDrivers;
	std::unordered_set<Target> mHasTarget;
};
} // namespace IG