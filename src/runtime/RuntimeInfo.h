#pragma once

#include "IG_Config.h"

namespace IG {
class RuntimeInfo {
public:
    [[nodiscard]] static Path executablePath();   // Path to the current executable
    [[nodiscard]] static Path modulePath();       // Path to the current module/dll
    [[nodiscard]] static Path modulePathAnyDSL(); // Path to the module/dll containing the AnyDSL runtime
    [[nodiscard]] static Path cacheDirectory();
    [[nodiscard]] static size_t cacheDirectorySize();

    [[nodiscard]] static std::vector<Path> splitEnvPaths(const std::string& str);
    [[nodiscard]] static std::string combineEnvPaths(const std::vector<Path>& paths);
};
} // namespace IG