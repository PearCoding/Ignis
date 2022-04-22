#include "RuntimeInfo.h"

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
} // namespace IG