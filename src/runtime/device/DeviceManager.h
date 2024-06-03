#pragma once

#include "IG_Config.h"
#include "SharedLibrary.h"
#include "Target.h"

namespace IG {
class IDeviceInterface;

class IG_LIB DeviceManager {
public:
    bool init(const Path& dir = Path{}, bool ignoreEnv = false, bool force = false);

    [[nodiscard]] const IDeviceInterface* getDevice(const TargetArchitecture& target);

    bool load(const TargetArchitecture& target);
    bool unload(const TargetArchitecture& target);
    bool unloadAll();

    [[nodiscard]] std::unordered_set<TargetArchitecture> availableDevices() const;

    static DeviceManager& instance();

private:
    bool addModule(const Path& path);
    bool loadModule(const Path& path);

    [[nodiscard]] void* getInterface(const SharedLibrary& lib) const;

    std::unordered_map<TargetArchitecture, SharedLibrary> mLoadedDevices;
    std::unordered_map<TargetArchitecture, Path> mAvailableDevices;
};

} // namespace IG