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

/// Linear RGBA Image with pixels in format [R, G, B, A] x Width x Height
struct Image {
    std::unique_ptr<float[]> pixels;
    size_t width, height;

    inline bool isValid() const { return pixels != nullptr; }

    /// Apply (inverse) gamma correction.
    /// Depending on the sRGB flag, can be 2.2 gamma or the sRGB gamma function
    void applyGammaCorrection(bool inverse = false, bool sRGB = false);

    /// Flip image in y-axis
    void flipY();

    /// Will format to packed format (RGBA, 8bit each)
    /// Use this only for byte formats, else image quality will be lost
    void copyToPackedFormat(std::vector<uint32>& dst) const; 

    /// Will be true if the image in path is not in float format
    static bool isPacked(const std::filesystem::path& path);

    /// Will be true if the image has an alpha channel
    static bool hasAlphaChannel(const std::filesystem::path& path);

    /// Loads a image in linear RGBA
    /// Supports EXR, HDR, PNG, JPEG and many other formats supported by the stbi library
    static Image load(const std::filesystem::path& path);

    /// Loads image and directly uploads to buffer in packed format
    /// Supports PNG, JPEG and many other formats supported by the stbi library
    /// Will not load EXR or HDR files, as they are not given in packed format
    static void loadAsPacked(const std::filesystem::path& path, std::vector<uint32>& dst, size_t& width, size_t& height);

    /// Save a image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    bool save(const std::filesystem::path& path);

    /// Save a image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    /// Given pointer should be linear in memory and should be in format [R, G, B, A] x width x height,
    /// With height being the major axis. If alpha channel is omitted as requested, the appointed format still is [R, G, B, A] x width x height
    static bool save(const std::filesystem::path& path, const float* rgba, size_t width, size_t height, bool skip_alpha = false);
};
} // namespace IG
