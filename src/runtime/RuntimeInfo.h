#pragma once

#include "IG_Config.h"

namespace IG {
class IG_LIB RuntimeInfo {
public:
    /// Path to the current executable.
    [[nodiscard]] static Path executablePath();
    /// Path to the ignis jit compiler 'igc'.
    [[nodiscard]] static Path igcPath();
    /// Path to 'libdevice.10.bc' needed by the cuda device.
    [[nodiscard]] static Path libdevicePath();
    /// Path to the module/dll for the given function.
    [[nodiscard]] static Path modulePath(void* func = nullptr);

    /// Path to the data folder alongside the framework. Contains like fonts, etc. Does not have to modifiable.
    [[nodiscard]] static Path readonlyDataPath();
    /// Path to a folder containing configuration files. This directory must be modifiable.
    [[nodiscard]] static Path configDirectory();
    /// Path to a folder containing cache files. This directory must be modifiable.
    [[nodiscard]] static Path cacheDirectory();

    /// Computes the size of the directory recursively (with all files inside of it).
    [[nodiscard]] static size_t sizeOfDirectory(const Path& dir);

    /// Splits the string to multiple path entries using the system dependant delimiter.
    [[nodiscard]] static std::vector<Path> splitEnvPaths(const std::string& str);
    /// Combines multiple paths to a single string using the system dependant delimiter.
    [[nodiscard]] static std::string combineEnvPaths(const std::vector<Path>& paths);
};
} // namespace IG