#pragma once

#include "IG_Config.h"

namespace IG {
class RuntimeInfo {
public:
    static std::filesystem::path executablePath();
    static std::filesystem::path cacheDirectory();
    static size_t cacheDirectorySize();

    static std::vector<std::filesystem::path> splitEnvPaths(const std::string& str);
    static std::string combineEnvPaths(const std::vector<std::filesystem::path>& paths);
};
} // namespace IG