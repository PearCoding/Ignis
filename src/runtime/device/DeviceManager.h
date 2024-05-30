#pragma once

#include "IG_Config.h"
#include "SharedLibrary.h"
#include "Target.h"

namespace IG {
class IDeviceInterface;

class DeviceManager {
public:
    bool init(const Path& dir = Path{}, bool ignoreEnv = false);

    IDeviceInterface* getDevice(const TargetArchitecture& target);

    bool load(const TargetArchitecture& target);
    bool unload(const TargetArchitecture& target);
    bool unloadAll();

    [[nodiscard]] std::vector<TargetArchitecture> availableDevices() const;

    static DeviceManager& instance();

private:
    bool addModule(const Path& path);
    bool loadModule(const Path& path);

    void* getInterface(const SharedLibrary& lib) const;

    std::unordered_map<TargetArchitecture, SharedLibrary, TargetArchitectureHash> mLoadedDevices;
    std::unordered_map<TargetArchitecture, Path, TargetArchitectureHash> mAvailableDevices;
};

} // namespace IG