#pragma once

#include "IG_Config.h"

namespace IG {
class IG_LIB RuntimeInfo {
public:
    [[nodiscard]] static Path executablePath();                 // Path to the current executable
    [[nodiscard]] static Path modulePath(void* func = nullptr); // Path to the module/dll for the given function
    [[nodiscard]] static Path dataPath();                       // Path to directory for data/caches
    [[nodiscard]] static Path cacheDirectory();
    [[nodiscard]] static size_t sizeOfDirectory(const Path& dir);

    [[nodiscard]] static std::vector<Path> splitEnvPaths(const std::string& str);
    [[nodiscard]] static std::string combineEnvPaths(const std::vector<Path>& paths);
};
} // namespace IG