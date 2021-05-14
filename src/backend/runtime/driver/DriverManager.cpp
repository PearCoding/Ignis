#include "DriverManager.h"
#include "Configuration.h"
#include "Logger.h"

#include <unordered_set>

#define _IG_DRIVER_ENV_PATH_NAME "IG_DRIVER_PATH"
#define _IG_DRIVER_LIB_PREFIX "ig_driver_"

namespace IG {
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
	constexpr char _IG_ENV_DELIMITER = ':';

	auto start = 0U;
	auto end   = str.find(_IG_ENV_DELIMITER);
	while (end != std::string::npos) {
		data.insert(std::filesystem::canonical(str.substr(start, end - start)));
		start = end + 1;
		end	  = str.find(_IG_ENV_DELIMITER, start);
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
			&& startsWith(entry.path().stem().string(), _IG_DRIVER_LIB_PREFIX))
			drivers.insert(entry.path());
	}

	return drivers;
}

bool DriverManager::init(const std::filesystem::path& dir, bool ignoreEnv)
{
	path_set paths;
	if (!ignoreEnv) {
		const char* envPaths = std::getenv(_IG_DRIVER_ENV_PATH_NAME);
		if (envPaths)
			split_env(envPaths, paths);
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

	if (mLoadedDrivers.empty()) {
		IG_LOG(L_ERROR) << "No driver could been found!" << std::endl;
		return false;
	}

	return true;
}

uint64 DriverManager::checkConfiguration(uint64 config) const
{
#define CHECK_RET(c)                 \
	if (mLoadedDrivers.count(c) > 0) \
	return c

	CHECK_RET(config);

	// Check without extra flags
	if (config & IG_C_NO_INSTANCES)
		return checkConfiguration(config & ~IG_C_NO_INSTANCES);

	// First check gpu pairs drivers/devices
	const uint64 devConfig = config & ~IG_C_MASK_DEVICE;
	if (config & IG_C_DEVICE_NVVM_STREAMING)
		CHECK_RET(devConfig | IG_C_DEVICE_NVVM_MEGA);
	if (config & IG_C_DEVICE_NVVM_MEGA)
		CHECK_RET(devConfig | IG_C_DEVICE_NVVM_STREAMING);
	if (config & IG_C_DEVICE_AMD_STREAMING)
		CHECK_RET(devConfig | IG_C_DEVICE_AMD_MEGA);
	if (config & IG_C_DEVICE_AMD_MEGA)
		CHECK_RET(devConfig | IG_C_DEVICE_AMD_STREAMING);

	CHECK_RET(devConfig | IG_C_DEVICE_AVX512);
	CHECK_RET(devConfig | IG_C_DEVICE_AVX2);
	CHECK_RET(devConfig | IG_C_DEVICE_AVX);
	CHECK_RET(devConfig | IG_C_DEVICE_SSE42);
	CHECK_RET(devConfig | IG_C_DEVICE_ASIMD);

	//TODO: What about camera and renderer? We assume they are always built!
	return devConfig | IG_C_DEVICE_GENERIC;
#undef CHECK_RET
}

bool DriverManager::load(uint64 config, DriverInterface& interface) const
{
	config = checkConfiguration(config);
	if (!mLoadedDrivers.count(config)) {
		IG_LOG(L_ERROR) << "No driver available!" << std::endl;
		return false; // No driver available!
	}

	const SharedLibrary& library = mLoadedDrivers.at(config);

	GetInterfaceFunction func = (GetInterfaceFunction)library.symbol("ig_get_interface");
	if (!func) {
		IG_LOG(L_FATAL) << "Could not find interface symbol for module after initialization!" << std::endl;
		return false;
	}

	interface = func();
	return true;
}

bool DriverManager::addModule(const std::filesystem::path& path)
{
	try {
		SharedLibrary library(path);

		GetInterfaceFunction func = (GetInterfaceFunction)library.symbol("ig_get_interface");
		if (!func) {
			IG_LOG(L_ERROR) << "Could not find interface symbol for module " << path << std::endl;
			return false;
		}

		const DriverInterface interface = func();

		// Silently replace
		mLoadedDrivers[interface.Configuration] = library;
		mHasTarget.insert(configurationToTarget(interface.Configuration));
	} catch (const std::exception& e) {
		IG_LOG(L_ERROR) << "Loading error for module " << path << ": " << e.what() << std::endl;
		return false;
	}
	return true;
}
} // namespace IG