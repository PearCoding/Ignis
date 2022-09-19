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
    [[nodiscard]] Target resolveTarget(Target target) const;
    bool load(Target target, DriverInterface& interface);
    [[nodiscard]] std::filesystem::path getPath(Target target) const;

    [[nodiscard]] bool hasCPU() const;
    [[nodiscard]] bool hasGPU() const;

    [[nodiscard]] Target recommendCPUTarget() const;
    [[nodiscard]] Target recommendGPUTarget() const;
    [[nodiscard]] Target recommendTarget() const;

private:
    bool addModule(const std::filesystem::path& path);
    std::unordered_map<Target, std::filesystem::path> mRegistredDrivers;
    std::unordered_map<Target, SharedLibrary> mLoadedDrivers;
};
} // namespace IG