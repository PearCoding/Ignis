#include "RuntimeInfo.h"
#include <sstream>

#ifdef IG_OS_LINUX
#include <climits>
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

namespace IG {
std::filesystem::path RuntimeInfo::executablePath()
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

#ifndef IG_OS_WINDOWS
constexpr char ENV_DELIMITER = ':';
#else
constexpr char ENV_DELIMITER = ';';
#endif

std::vector<std::filesystem::path> RuntimeInfo::splitEnvPaths(const std::string& str)
{
    std::vector<std::filesystem::path> paths;

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

std::string RuntimeInfo::combineEnvPaths(const std::vector<std::filesystem::path>& paths)
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