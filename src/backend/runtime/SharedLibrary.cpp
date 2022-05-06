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
    explicit SharedLibraryInternal(const std::string& path)
        : Handle(dlopen(path.c_str(), RTLD_LAZY))
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
            return {}; // No error message has been recorded

        LPSTR messageBuffer = nullptr;

        // Ask Win32 to give us the string version of that message ID.
        // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        // Copy the error message into a std::string.
        std::string message(messageBuffer, size);

        // Free the Win32's string's buffer.
        LocalFree(messageBuffer);

        return message;
    }

    explicit SharedLibraryInternal(const std::string& path)
        : Handle(LoadLibraryA(path.c_str()))
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
{
    const std::string u8 = file.u8string();

#ifdef USE_DLOPEN
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