#include "RuntimeInfo.h"
#include "Logger.h"
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
#include <shlobj_core.h>
#else
#error DLL implementation missing
#endif

#include "device/Target.h"

#define APP_NAME "Ignis"

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

Path RuntimeInfo::modulePath(void* func)
{
    if (func == nullptr)
        func = (void*)&Target::pickGPU;

#if defined(IG_OS_LINUX) || defined(IG_OS_APPLE)
#if __USE_GNU
    Dl_info dl_info;
    dladdr(func, &dl_info);
    return (dl_info.dli_fname);
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

Path RuntimeInfo::cacheDirectory()
{
#if defined(IG_OS_LINUX)
    const char* cache_home = std::getenv("XDG_CACHE_HOME");
    if (cache_home == nullptr) {
        const char* home = std::getenv("HOME");
        if (home == nullptr) {
            const auto dir = executablePath();
            if (dir.empty())
                return modulePath().parent_path() / "cache";
            else
                return dir.parent_path() / "cache";
        }
        return Path(home) / ".cache" / APP_NAME;
    }
    return Path(cache_home) / APP_NAME;
#elif defined(IG_OS_APPLE)
    // TODO: Better path?
    const auto dir = executablePath();
    if (dir.empty())
        return modulePath().parent_path() / "cache";
    else
        return dir.parent_path() / "cache";
#elif defined(IG_OS_WINDOWS)
    wchar_t* path = nullptr;
    HRESULT res   = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);

    if (SUCCEEDED(res)) {
        Path full_path = path;
        CoTaskMemFree(static_cast<PWSTR>(path));
        return full_path / APP_NAME / "cache";
    } else {
        return {};
    }
#endif
}

size_t RuntimeInfo::sizeOfDirectory(const Path& dir)
{
    if (!std::filesystem::exists(dir))
        return 0;

    // Non-recursive as the cache directory should be flat
    size_t size = 0;
    for (std::filesystem::directory_entry const& entry :
         std::filesystem::directory_iterator(dir)) {
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
        stream << paths[i].generic_string();
        if (i < paths.size() - 1)
            stream << ENV_DELIMITER;
    }

    return stream.str();
}
} // namespace IG