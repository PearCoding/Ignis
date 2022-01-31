#pragma once

#include "IG_Config.h"

#include <exception>
#include <sstream>

namespace IG {
class ImageLoadException : public std::exception {
public:
    ImageLoadException(const std::string& message, const std::filesystem::path& path)
        : std::exception()
        , mMessage(message)
        , mPath(path)
    {
        std::stringstream stream;
        stream << mMessage << " (" << mPath << ")";
        mFullMessage = stream.str();
    }

    inline const char* what() const throw() override
    {
        return mFullMessage.c_str();
    }

    inline const std::filesystem::path& path() const
    {
        return mPath;
    }

private:
    std::string mMessage;
    std::filesystem::path mPath;
    std::string mFullMessage;
};
using ImageSaveException = ImageLoadException;

struct ImageRgba32 {
    std::unique_ptr<float[]> pixels;
    size_t width, height;

    inline bool isValid() const { return pixels != nullptr; }
    void applyGammaCorrection();
    void flipY();

    static ImageRgba32 load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path);
    static bool save(const std::filesystem::path& path, const float* rgba, size_t width, size_t height, bool skip_alpha = false);
};
} // namespace IG
