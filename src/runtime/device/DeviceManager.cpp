#include "DeviceManager.h"
#include "IDeviceInterface.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "StringUtils.h"

#include <unordered_set>

#define _IG_DEVICE_ENV_PATH_NAME "IG_DEVICE_PATH"
#define _IG_DEVICE_ENV_SKIP_SYSTEM_PATH "IG_DEVICE_SKIP_SYSTEM_PATH"
#define _IG_DEVICE_LIB_PREFIX "ig_device_"

namespace IG {

using GetInterfaceFunction = const IDeviceInterface* (*)();

struct path_hash {
    std::size_t operator()(const Path& path) const
    {
        return std::filesystem::hash_value(path);
    }
};

using path_set = std::unordered_set<Path, path_hash>;

inline static void split_env(const std::string& str, path_set& data)
{
    constexpr char _IG_ENV_DELIMITER = ':';

    size_t start = 0U;
    size_t end   = str.find(_IG_ENV_DELIMITER);
    while (end != std::string::npos) {
        data.insert(std::filesystem::canonical(str.substr(start, end - start)));
        start = end + 1;
        end   = str.find(_IG_ENV_DELIMITER, start);
    }

    if (end != start)
        data.insert(std::filesystem::canonical(str.substr(start, end)));
}

static path_set getDevicesFromPath(const std::filesystem::path& path)
{
    path_set drivers;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file())
            continue;
        if (SharedLibrary::isSharedLibrary(entry.path())
#ifdef IG_DEBUG // Only debug builds
            && string_ends_with(entry.path().stem().string(), "_d")
#else
            && !string_ends_with(entry.path().stem().string(), "_d")
#endif
            && string_starts_with(entry.path().stem().string(), _IG_DEVICE_LIB_PREFIX))
            drivers.insert(entry.path());
    }

    return drivers;
}

bool DeviceManager::init(const Path& dir, bool ignoreEnv, bool force)
{
    // Check if an initialization is required
    if (!force && !mAvailableDevices.empty())
        return true;

    path_set paths;

    bool skipSystem = false; // Skip the system search path
    if (!ignoreEnv) {
        const char* envPaths = std::getenv(_IG_DEVICE_ENV_PATH_NAME);
        if (envPaths)
            split_env(envPaths, paths);

        if (std::getenv(_IG_DEVICE_ENV_SKIP_SYSTEM_PATH))
            skipSystem = true;
    }

    if (!skipSystem) {
        const auto exePath = RuntimeInfo::executablePath();
        const auto libPath = exePath.parent_path().parent_path() / "lib";
        paths.insert(libPath);
    }

    if (!dir.empty())
        paths.insert(std::filesystem::canonical(dir));

    for (auto& path : paths) {
        IG_LOG(L_DEBUG) << "Searching for devices in " << path << std::endl;
        for (auto& device_path : getDevicesFromPath(path)) {
            IG_LOG(L_DEBUG) << "Adding device " << device_path << std::endl;

            if (!addModule(device_path))
                return false;
        }
    }

    if (mAvailableDevices.empty()) {
        IG_LOG(L_ERROR) << "No device could been found!" << std::endl;
        return false;
    }

    return true;
}

const IDeviceInterface* DeviceManager::getDevice(const TargetArchitecture& target)
{
    if (!load(target))
        return nullptr;

    if (auto it = mLoadedDevices.find(target); it != mLoadedDevices.end()) {
        GetInterfaceFunction func = (GetInterfaceFunction)getInterface(it->second);
        if (func)
            return func();
    }

    return nullptr;
}

bool DeviceManager::load(const TargetArchitecture& target)
{
    if (mLoadedDevices.count(target) > 0)
        return true;

    const auto it = mAvailableDevices.find(target);
    if (it == mAvailableDevices.end()) {
        IG_LOG(L_ERROR) << "Could not load device due to not being available." << std::endl;
        return false;
    }

    return loadModule(it->second);
}

bool DeviceManager::unload(const TargetArchitecture& target)
{
    auto it = mLoadedDevices.find(target);
    if (it == mLoadedDevices.end()) // Already not loaded -> all good
        return true;

    it->second.unload();
    mLoadedDevices.erase(it);
    return true;
}

bool DeviceManager::unloadAll()
{
    for (const auto& pair : mAvailableDevices) {
        if (!unload(pair.first))
            return false;
    }

    return true;
}

std::vector<TargetArchitecture> DeviceManager::availableDevices() const
{
    std::vector<TargetArchitecture> list;
    list.reserve(mAvailableDevices.size());

    for (const auto& pair : mAvailableDevices)
        list.push_back(pair.first);

    return list;
}

static std::optional<TargetArchitecture> checkModule(const Path& path, const IDeviceInterface* interface)
{
    const TargetArchitecture deviceTarget = interface->getArchitecture();
    const Build::Version deviceVersion    = interface->getVersion();

    const Build::Version runtimeVersion = Build::getVersion();

    if (deviceVersion != runtimeVersion) {
        IG_LOG(L_WARNING) << "Skipping module " << path << " as the provided version " << deviceVersion.Major << "." << deviceVersion.Minor
                          << " does not match the runtime version " << runtimeVersion.Major << "." << runtimeVersion.Minor << std::endl;
        return std::nullopt;
    }

    return deviceTarget;
}

bool DeviceManager::addModule(const Path& path)
{
    try {
        SharedLibrary library(path);

        GetInterfaceFunction func = (GetInterfaceFunction)getInterface(library);
        if (!func)
            return false;

        const IDeviceInterface* interface = func();
        const auto target                 = checkModule(path, interface);
        if (!target.has_value())
            return false;

        mAvailableDevices[*target] = path;

        library.unload();

    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << "Loading error for module " << path << " when adding to the list of available devices: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DeviceManager::loadModule(const Path& path)
{
    try {
        SharedLibrary library(path);

        GetInterfaceFunction func = (GetInterfaceFunction)getInterface(library);
        if (!func)
            return false;

        const IDeviceInterface* interface = func();
        const auto target                 = checkModule(path, interface);
        if (!target.has_value())
            return false;

        mLoadedDevices[*target] = library;
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << "Loading error for module " << path << " when adding to the list of available devices: " << e.what() << std::endl;
        return false;
    }
    return true;
}

void* DeviceManager::getInterface(const SharedLibrary& lib) const
{
    GetInterfaceFunction func = (GetInterfaceFunction)lib.symbol("ig_get_interface");
    if (!func) {
        IG_LOG(L_ERROR) << "Could not find interface symbol for module " << lib.path() << std::endl;
        return nullptr;
    }

    return func;
}

DeviceManager& DeviceManager::instance()
{
    static DeviceManager manager;
    return manager;
}

} // namespace IG