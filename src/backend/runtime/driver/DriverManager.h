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
	Target resolveTarget(Target target) const;
	bool load(Target target, DriverInterface& interface) const;
	std::filesystem::path getPath(Target target) const;

private:
	bool addModule(const std::filesystem::path& path);
	std::unordered_map<Target, SharedLibrary> mLoadedDrivers;
};
} // namespace IG