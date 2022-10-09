#pragma once

#include "IG_Config.h"

namespace IG {
class RuntimeInfo {
public:
    [[nodiscard]] static std::filesystem::path executablePath(); // Path to the current executable
    [[nodiscard]] static std::filesystem::path modulePath();     // Path to the current module/dll
    [[nodiscard]] static std::filesystem::path cacheDirectory();
    [[nodiscard]] static size_t cacheDirectorySize();

    [[nodiscard]] static std::vector<std::filesystem::path> splitEnvPaths(const std::string& str);
    [[nodiscard]] static std::string combineEnvPaths(const std::vector<std::filesystem::path>& paths);
};
} // namespace IG