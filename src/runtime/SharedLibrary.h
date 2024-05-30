#pragma once

#include "IG_Config.h"

namespace IG {
class SharedLibrary {
public:
    SharedLibrary();
    SharedLibrary(const Path& file);
    ~SharedLibrary();

    inline const Path& path() const { return mPath; }

    void* symbol(const std::string& name) const;
    void unload();

    inline operator bool() const { return mInternal != nullptr; }

    static bool isSharedLibrary(const Path& path);

private:
    Path mPath;
    std::shared_ptr<struct SharedLibraryInternal> mInternal;
};
} // namespace IG