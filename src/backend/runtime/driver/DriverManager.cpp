#include "DriverManager.h"
#include "Logger.h"
#include "RuntimeInfo.h"
#include "config/Version.h"

#include <algorithm>
#include <numeric>
#include <unordered_set>

namespace IG {
constexpr const char* const DRIVER_ENV_PATH_NAME        = "IG_DRIVER_PATH";
constexpr const char* const DRIVER_ENV_SKIP_SYSTEM_PATH = "IG_DRIVER_SKIP_SYSTEM_PATH";
constexpr const char* const DRIVER_LIB_PREFIX           = "ig_driver_";

using GetInterfaceFunction = DriverInterface (*)();

struct path_hash {
    std::size_t operator()(const std::filesystem::path& path) const
    {
        return std::filesystem::hash_value(path);
    }
};

using path_set = std::unordered_set<std::filesystem::path, path_hash>;

inline static void split_env(const std::string& str, path_set& data)
{
    constexpr char ENV_DELIMITER = ':';

    size_t start = 0;
    size_t end   = str.find(ENV_DELIMITER);
    while (end != std::string::npos) {
        data.insert(std::filesystem::canonical(str.substr(start, end - start)));
        start = end + 1;
        end   = str.find(ENV_DELIMITER, start);
    }

    if (end != start)
        data.insert(std::filesystem::canonical(str.substr(start, end)));
}

inline static bool isSharedLibrary(const std::filesystem::path& path)
{
    return path.extension() == ".so" || path.extension() == ".dll";
}

inline static bool endsWith(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

inline static bool startsWith(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

static path_set getDriversFromPath(const std::filesystem::path& path)
{
    path_set drivers;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!entry.is_regular_file())
            continue;
        if (isSharedLibrary(entry.path())
#ifdef IG_DEBUG // Only debug builds
            && endsWith(entry.path().stem().string(), "_d")
#else
            && !endsWith(entry.path().stem().string(), "_d")
#endif
            && startsWith(entry.path().stem().string(), DRIVER_LIB_PREFIX))
            drivers.insert(entry.path());
    }

    return drivers;
}

bool DriverManager::init(const std::filesystem::path& dir, bool ignoreEnv)
{
    path_set paths;

    bool skipSystem = false; // Skip the system search path
    if (!ignoreEnv) {
        const char* envPaths = std::getenv(DRIVER_ENV_PATH_NAME);
        if (envPaths)
            split_env(envPaths, paths);

        if (std::getenv(DRIVER_ENV_SKIP_SYSTEM_PATH))
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
        IG_LOG(L_DEBUG) << "Searching for drivers in " << path << std::endl;
        for (auto& driver_path : getDriversFromPath(path)) {
            IG_LOG(L_DEBUG) << "Adding driver " << driver_path << std::endl;
            if (!addModule(driver_path))
                return false;
        }
    }

    if (mRegistredDrivers.empty()) {
        IG_LOG(L_ERROR) << "No driver could been found!" << std::endl;
        return false;
    }

    return true;
}

Target DriverManager::resolveTarget(Target target) const
{
    if (mRegistredDrivers.count(target) > 0)
        return target;
    else if (mRegistredDrivers.count(Target::AVX512) > 0)
        return Target::AVX512;
    else if (mRegistredDrivers.count(Target::AVX2) > 0)
        return Target::AVX2;
    else if (mRegistredDrivers.count(Target::AVX) > 0)
        return Target::AVX;
    else if (mRegistredDrivers.count(Target::SSE42) > 0)
        return Target::SSE42;
    else if (mRegistredDrivers.count(Target::ASIMD) > 0)
        return Target::ASIMD;
    else
        return Target::GENERIC;
}

bool DriverManager::load(Target target, DriverInterface& interface)
{
    target = resolveTarget(target);
    if (!mRegistredDrivers.count(target)) {
        IG_LOG(L_ERROR) << "No driver available!" << std::endl;
        return false; // No driver available!
    }

    mLoadedDrivers[target]       = SharedLibrary(mRegistredDrivers.at(target));
    const SharedLibrary& library = mLoadedDrivers.at(target);

    auto func = reinterpret_cast<GetInterfaceFunction>(library.symbol("ig_get_interface"));
    if (!func) {
        IG_LOG(L_FATAL) << "Could not find interface symbol for module after initialization!" << std::endl;
        return false;
    }

    interface = func();
    return true;
}

std::filesystem::path DriverManager::getPath(Target target) const
{
    target = resolveTarget(target);
    if (!mRegistredDrivers.count(target))
        return {}; // No driver available!

    return mRegistredDrivers.at(target);
}

bool DriverManager::addModule(const std::filesystem::path& path)
{
    try {
        SharedLibrary library(path);

        auto func = reinterpret_cast<GetInterfaceFunction>(library.symbol("ig_get_interface"));
        if (!func) {
            IG_LOG(L_ERROR) << "Could not find interface symbol for module " << path << std::endl;
            return false;
        }

        const DriverInterface interface = func();

        if (interface.MajorVersion != IG_VERSION_MAJOR || interface.MinorVersion != IG_VERSION_MINOR) {
            IG_LOG(L_WARNING) << "Skipping module " << path << " as the provided version " << interface.MajorVersion << "." << interface.MinorVersion
                              << " does not match the runtime version " << IG_VERSION_MAJOR << "." << IG_VERSION_MINOR << std::endl;
            return false;
        }

        if (mRegistredDrivers.count(interface.Target) > 0)
            IG_LOG(L_WARNING) << "Module " << path << " is replacing another module for present target " << targetToString(interface.Target) << std::endl;

        mRegistredDrivers[interface.Target] = path;
    } catch (const std::exception& e) {
        IG_LOG(L_ERROR) << "Loading error for module " << path << ": " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DriverManager::hasCPU() const
{
    return std::any_of(mRegistredDrivers.begin(), mRegistredDrivers.end(),
                       [](const std::pair<Target, std::filesystem::path>& p) { return isCPU(p.first); });
}

bool DriverManager::hasGPU() const
{
    return std::any_of(mRegistredDrivers.begin(), mRegistredDrivers.end(),
                       [](const std::pair<Target, std::filesystem::path>& p) { return !isCPU(p.first); });
}

static int costFunction(Target target)
{
    // Note: The cost numbers are arbitary choosen...
    switch (target) {
    case Target::INVALID:
        return 10000000;
    default:
    case Target::GENERIC:
        return 100;
    case Target::ASIMD:
        return 50;
    case Target::SSE42:
        return 25;
    case Target::AVX:
        return 10;
    case Target::AVX2:
        return 5;
    case Target::AVX512:
        return 3;
    case Target::AMDGPU:
    case Target::NVVM:
        return 1;
    }
}

Target DriverManager::recommendCPUTarget() const
{
    return std::accumulate(mRegistredDrivers.begin(), mRegistredDrivers.end(),
                           Target::GENERIC,
                           [](const Target& a, const std::pair<Target, std::filesystem::path>& b) {
                               return (isCPU(b.first) && costFunction(b.first) < costFunction(a)) ? b.first : a;
                           });
}

Target DriverManager::recommendGPUTarget() const
{
    return std::accumulate(mRegistredDrivers.begin(), mRegistredDrivers.end(),
                           Target::GENERIC,
                           [](const Target& a, const std::pair<Target, std::filesystem::path>& b) {
                               return (!isCPU(b.first) && costFunction(b.first) < costFunction(a)) ? b.first : a;
                           });
}

Target DriverManager::recommendTarget() const
{
    return std::accumulate(mRegistredDrivers.begin(), mRegistredDrivers.end(),
                           Target::GENERIC,
                           [](const Target& a, const std::pair<Target, std::filesystem::path>& b) {
                               return (costFunction(b.first) < costFunction(a)) ? b.first : a;
                           });
}

} // namespace IG