#include "RuntimeInfo.h"
#include "Logger.h"
#include "device/AnyDSLRuntime.h"

#include <sstream>

#ifdef IG_OS_LINUX
#include <climits>
#include <dlfcn.h>
#include <unistd.h>
#elif defined(IG_OS_APPLE)
#include <climits>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#elif defined(IG_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#error DLL implementation missing
#endif

#include "device/Target.h"

namespace IG {
Path RuntimeInfo::executablePath()
{
#ifdef IG_OS_LINUX
    constexpr const char* const PROC_LINK = "/proc/self/exe";
    std::array<char, PATH_MAX> linkname{};
    ssize_t r = readlink(PROC_LINK, linkname.data(), linkname.size());

    if (r < 0) {
        perror("readlink");
        return {};
    }

    if (r > PATH_MAX) {
        return {};
    }

    return std::string(linkname.data(), r);
#elif defined(IG_OS_APPLE)
    std::array<char, PATH_MAX> linkname{};
    uint32_t size = (uint32_t)linkname.size();
    ssize_t r     = _NSGetExecutablePath(linkname.data(), &size);

    if (r < 0) {
        return {};
    }

    return std::string(linkname.data());
#elif defined(IG_OS_WINDOWS)
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
#endif
}

static inline Path getModulePath(void* func)
{
#if defined(IG_OS_LINUX) || defined(IG_OS_APPLE)
#if __USE_GNU
    Dl_info dl_info;
    dladdr(func, &dl_info);
    return dl_info.dli_fname;
#else
    return {}; // TODO
#endif
#elif defined(IG_OS_WINDOWS)
    wchar_t path[MAX_PATH] = { 0 };
    HMODULE hm             = NULL;

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)func, &hm)
        == 0) {
        int ret = GetLastError();
        IG_LOG(L_ERROR) << "GetModuleHandleExW failed, error = " << ret << std::endl;
        return {};
    }

    if (GetModuleFileNameW(hm, path, MAX_PATH) == 0) {
        int ret = GetLastError();
        IG_LOG(L_ERROR) << "GetModuleFileNameW failed, error = " << ret << std::endl;
        return {};
    }
    return path;
#endif
}

Path RuntimeInfo::modulePath()
{
    return getModulePath((void*)&Target::pickGPU);
}

Path RuntimeInfo::modulePathAnyDSL()
{
    return getModulePath((void*)&anydslGetDevice);
}

Path RuntimeInfo::cacheDirectory()
{
    auto exe = modulePath();
    if (exe.empty())
        exe = executablePath();

    const auto dir = exe.parent_path();

    if (dir.empty())
        return {};

    return dir / "cache";
}

size_t RuntimeInfo::cacheDirectorySize()
{
    const auto cacheDir = cacheDirectory();

    if (!std::filesystem::exists(cacheDir))
        return 0;

    // Non-recursive as the cache directory should be flat
    size_t size = 0;
    for (std::filesystem::directory_entry const& entry :
         std::filesystem::directory_iterator(cacheDir)) {
        if (entry.is_regular_file())
            size += entry.file_size();
    }

    return size;
}

#ifndef IG_OS_WINDOWS
constexpr char ENV_DELIMITER = ':';
#else
constexpr char ENV_DELIMITER = ';';
#endif

std::vector<Path> RuntimeInfo::splitEnvPaths(const std::string& str)
{
    std::vector<Path> paths;

    size_t start = 0;
    size_t end   = str.find(ENV_DELIMITER);
    while (end != std::string::npos) {
        paths.push_back(std::filesystem::canonical(str.substr(start, end - start)));
        start = end + 1;
        end   = str.find(ENV_DELIMITER, start);
    }

    if (end != start)
        paths.push_back(std::filesystem::canonical(str.substr(start, end)));

    return paths;
}

std::string RuntimeInfo::combineEnvPaths(const std::vector<Path>& paths)
{
    std::stringstream stream;

    for (size_t i = 0; i < paths.size(); ++i) {
        stream << paths[i].u8string();
        if (i < paths.size() - 1)
            stream << ENV_DELIMITER;
    }

    return stream.str();
}
} // namespace IG