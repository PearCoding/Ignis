#include "SharedLibrary.h"

#if defined(IG_OS_LINUX) || defined(IG_OS_APPLE)
#define USE_DLOPEN
#include <dlfcn.h>
#elif defined(IG_OS_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#error DLL implementation missing
#endif

namespace IG {
class SharedLibraryInternal {
    IG_CLASS_NON_COPYABLE(SharedLibraryInternal);
    IG_CLASS_NON_MOVEABLE(SharedLibraryInternal);

public:
#ifdef USE_DLOPEN
    void* Handle;
#elif defined(IG_OS_WINDOWS)
    HINSTANCE Handle;
#endif

#ifdef USE_DLOPEN
    explicit SharedLibraryInternal(const std::filesystem::path& path)
        : Handle(dlopen(path.u8string().c_str(), RTLD_LAZY))
    {
        if (!Handle)
            throw std::runtime_error(dlerror());
    }
#elif defined(IG_OS_WINDOWS)
    static inline std::string GetLastErrorAsString()
    {
        // Get the error message ID, if any.
        DWORD errorMessageID = ::GetLastError();
        if (errorMessageID == 0)
            return {};

        // Format to string
        LPSTR messageBuffer = nullptr;
        size_t size         = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                             NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        // Map to std::string
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        return message;
    }

    explicit SharedLibraryInternal(const std::filesystem::path& path)
        : Handle(LoadLibraryW(path.c_str()))
    {
        if (!Handle)
            throw std::runtime_error(GetLastErrorAsString());
    }
#endif

    ~SharedLibraryInternal()
    {
#ifdef USE_DLOPEN
        dlclose(Handle);
#elif defined(IG_OS_WINDOWS)
        FreeLibrary(Handle);
#endif
    }
};

SharedLibrary::SharedLibrary(const std::filesystem::path& file)
    : mPath(file)
#ifdef USE_DLOPEN
    , mInternal(new SharedLibraryInternal(file))
#elif defined(IG_OS_WINDOWS)
    , mInternal(new SharedLibraryInternal(file))
#endif
{
}

void* SharedLibrary::symbol(const std::string& name) const
{
    if (!mInternal)
        return nullptr;

#ifdef USE_DLOPEN
    return dlsym(mInternal->Handle, name.c_str());
#elif defined(IG_OS_WINDOWS)
    return GetProcAddress(mInternal->Handle, name.c_str());
#endif
}

void SharedLibrary::unload()
{
    mInternal.reset();
}
} // namespace IG