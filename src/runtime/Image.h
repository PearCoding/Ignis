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

    [[nodiscard]] inline const char* what() const throw() override
    {
        return mFullMessage.c_str();
    }

    [[nodiscard]] inline const std::filesystem::path& path() const
    {
        return mPath;
    }

private:
    std::string mMessage;
    std::filesystem::path mPath;
    std::string mFullMessage;
};
using ImageSaveException = ImageLoadException;

struct ImageMetaData;
/// Linear image with pixels in format Channels x Width x Height
/// Only supports RGBA and R images
/// RGB will be expanded to RGBA for access performance
struct IG_LIB Image {
    std::unique_ptr<float[]> pixels;
    size_t width, height, channels;

    [[nodiscard]] inline bool isValid() const { return pixels != nullptr; }
    [[nodiscard]] inline bool isMono() const { return channels == 1; }

    /// Apply (inverse) gamma correction.
    /// Depending on the sRGB flag, can be 2.2 gamma or the sRGB gamma function
    void applyGammaCorrection(bool inverse = false, bool sRGB = false);

    /// Flip image in y-axis
    void flipY();

    /// Will format to packed format (RGBA or Mono, 8bit each)
    /// Use this only for byte formats, else image quality will be lost
    void copyToPackedFormat(std::vector<uint8>& dst) const;

    /// Will be true if the image in path is not in float format
    [[nodiscard]] static bool isPacked(const std::filesystem::path& path);

    /// Will return the number of channels if loaded with `load`
    [[nodiscard]] static size_t extractChannelCount(const std::filesystem::path& path);

    /// Loads a image in linear RGBA
    /// Supports EXR, HDR, PNG, JPEG and many other formats supported by the stbi library
    [[nodiscard]] static Image load(const std::filesystem::path& path, ImageMetaData* metaData = nullptr);

    /// Loads image and directly uploads to buffer in packed format
    /// Supports PNG, JPEG and many other formats supported by the stbi library
    /// Will not load EXR or HDR files, as they are not given in packed format
    static void loadAsPacked(const std::filesystem::path& path, std::vector<uint8>& dst, size_t& width, size_t& height, size_t& channels, bool linear);

    /// Save a image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    bool save(const std::filesystem::path& path);

    /// Save a image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    /// Given pointer should be linear in memory and should be in format channels x width x height,
    /// With height being the major axis.
    /// If channels == 4 and alpha channel is omitted as requested, the appointed format still is channels x width x height
    static bool save(const std::filesystem::path& path, const float* data, size_t width, size_t height, size_t channels, bool skip_alpha = false);

    static Image createSolidImage(const Vector4f& color, size_t width = 1, size_t height = 1);
};
} // namespace IG
