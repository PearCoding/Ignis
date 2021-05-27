#include "RuntimeInfo.h"

#ifdef IG_OS_LINUX
#include <limits.h>
#include <unistd.h>
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
#define _PROC_LINK "/proc/self/exe"
	char linkname[PATH_MAX];
	ssize_t r = readlink(_PROC_LINK, linkname, PATH_MAX);

	if (r < 0) {
		perror("readlink");
		return {};
	}

	if (r > PATH_MAX) {
		return {};
	}

	return std::string(linkname, r);

#elif defined(IG_OS_WINDOWS)
	wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
#endif
}
} // namespace IG