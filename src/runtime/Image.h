#pragma once

#include "IG_Config.h"

#include <exception>
#include <sstream>

namespace IG {
class ImageLoadException : public std::exception {
public:
    ImageLoadException(const std::string& message, const Path& path)
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

    [[nodiscard]] inline const Path& path() const
    {
        return mPath;
    }

private:
    std::string mMessage;
    Path mPath;
    std::string mFullMessage;
};
using ImageSaveException = ImageLoadException;

/// Currently supported meta data information
struct ImageMetaData {
    std::optional<Vector3f> CameraEye;
    std::optional<Vector3f> CameraUp;
    std::optional<Vector3f> CameraDir;
    std::optional<size_t> Seed;
    std::optional<size_t> SamplePerPixel;
    std::optional<size_t> SamplePerIteration;
    std::optional<size_t> Iteration;
    std::optional<size_t> Frame;
    std::optional<size_t> RendertimeInMilliseconds;
    std::optional<size_t> RendertimeInSeconds;
    std::optional<std::string> TechniqueType;
    std::optional<std::string> CameraType;
    std::optional<std::string> TargetString;

    std::unordered_map<std::string, std::string> CustomStrings;
    std::unordered_map<std::string, float> CustomFloats;
    std::unordered_map<std::string, size_t> CustomIntegers;
    std::unordered_map<std::string, Vector2f> CustomVec2s;
    std::unordered_map<std::string, Vector3f> CustomVec3s;
};

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

    /// Apply exposure and offset
    /// In contrary to proper tonemapping, this applies for each component individually
    void applyExposureOffset(float exposure, float offset = 0);

    /// Flip image in y-axis
    void flipY();

    [[nodiscard]] Vector4f computeAverage() const;

    enum class FilterMethod {
        Nearest,
        Bilinear,
        Bicubic
    };

    enum class BorderMethod {
        Clamp,
        Repeat,
        Mirror
    };

    /// Basic evaluation of the image at point `uv`.
    [[nodiscard]] Vector4f eval(const Vector2f& uv, BorderMethod borderMethod = BorderMethod::Repeat, FilterMethod filterMethod = FilterMethod::Bicubic) const;

    /// Will format to packed format (RGBA or Mono, 8bit each)
    /// Use this only for byte formats, else image quality will be lost
    void copyToPackedFormat(std::vector<uint8>& dst) const;

    /// Will be true if the image in path is not in float format
    [[nodiscard]] static bool isPacked(const Path& path);

    /// Loads an image in linear RGBA
    /// Supports EXR, HDR, PNG, JPEG and many other formats supported by the stbi library. In case of EXR only the color layer will be loaded.
    [[nodiscard]] static Image load(const Path& path, ImageMetaData* metaData = nullptr);

    /// Loads image and directly uploads to buffer in packed format
    /// Supports PNG, JPEG and many other formats supported by the stbi library
    /// Will not load EXR or HDR files, as they are not given in packed format
    static void loadAsPacked(const Path& path, std::vector<uint8>& dst, size_t& width, size_t& height, size_t& channels, bool linear);

    /// Save an image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    bool save(const Path& path, const ImageMetaData* metaData = nullptr);

    /// Save an image in linear RGBA in EXR format
    /// No other format is supported, except EXR. The file should end with .exr
    /// Given pointer should be linear in memory and should be in format channels x width x height,
    /// With height being the major axis.
    /// If channels == 4 and alpha channel is omitted as requested, the appointed format still is channels x width x height
    static bool save(const Path& path, const float* data, size_t width, size_t height, size_t channels, bool skip_alpha = false, const ImageMetaData* metaData = nullptr);

    /// Save an image in linear EXR format with custom layers
    /// No other format is supported, except EXR. The file should end with .exr
    /// Given pointer should be linear in memory and should be in format channels x width x height,
    /// With height being the major axis.
    static bool save(const Path& path, size_t width, size_t height, const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names, const ImageMetaData* metaData = nullptr);

    struct Resolution {
        size_t Width;
        size_t Height;
        size_t Channels;
    };
    /// Tries to get the resolution without loading image data. Will throw ImageLoadException if image is not available
    [[nodiscard]] static Resolution loadResolution(const Path& path);

    [[nodiscard]] static Image createSolidImage(const Vector4f& color, size_t width = 1, size_t height = 1);
};
} // namespace IG
