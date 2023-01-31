#pragma once

#include "IG_Config.h"

namespace IG {
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
    std::optional<size_t> RendertimeInSeconds;
    std::optional<std::string> TechniqueType;
    std::optional<std::string> CameraType;
    std::optional<std::string> TargetString;
};

class IG_LIB ImageIO {
public:
    static bool save(const std::filesystem::path& path, size_t width, size_t height,
                     const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names,
                     const ImageMetaData& metaData = ImageMetaData());
};
} // namespace IG
