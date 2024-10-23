#include "SharedLibrary.h"

#ifdef IG_OS_LINUX
#include <dlfcn.h>
#elif defined(IG_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#error DLL implementation missing
#endif

namespace IG {
struct SharedLibraryInternal {
#ifdef IG_OS_LINUX
    void* Handle;
#elif defined(IG_OS_WINDOWS)
    HINSTANCE Handle;
#endif

#ifdef IG_OS_LINUX
    explicit SharedLibraryInternal(const std::string& path)
        : Handle(dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL))
    {
        if (!Handle)
            throw std::runtime_error(dlerror());
    }
#elif defined(IG_OS_WINDOWS)
    explicit SharedLibraryInternal(const std::string& path)
        : Handle(LoadLibraryA(path.c_str()))
    {
        if (!Handle)
            throw std::runtime_error("Could not load library: " + std::system_category().message(GetLastError()));
    }
#endif

    ~SharedLibraryInternal()
    {
#ifdef IG_OS_LINUX
        dlclose(Handle);
#elif defined(IG_OS_WINDOWS)
        FreeLibrary(Handle);
#endif
    }
};

SharedLibrary::SharedLibrary() {}

SharedLibrary::SharedLibrary(const Path& file)
    : mPath(file)
{
    const std::string u8 = file.generic_string();

#ifdef IG_OS_LINUX
    try {
        mInternal.reset(new SharedLibraryInternal(u8 + ".so"));
    } catch (...) {
        mInternal.reset(new SharedLibraryInternal(u8));
    }
#elif defined(IG_OS_WINDOWS)
    try {
        mInternal.reset(new SharedLibraryInternal(u8 + ".dll"));
    } catch (...) {
        mInternal.reset(new SharedLibraryInternal(u8));
    }
#endif
}

SharedLibrary::~SharedLibrary() {}

void* SharedLibrary::symbol(const std::string& name) const
{
    if (!mInternal)
        return nullptr;

#ifdef IG_OS_LINUX
std::cout  << mInternal->Handle << " " << name << std::endl;
    return dlsym(mInternal->Handle, name.c_str());
#elif defined(IG_OS_WINDOWS)
    return GetProcAddress(mInternal->Handle, name.c_str());
#endif
}

void SharedLibrary::unload()
{
    mInternal.reset();
}

bool SharedLibrary::isSharedLibrary(const Path& path)
{
    return path.extension() == ".so" || path.extension() == ".dll";
}
} // namespace IG