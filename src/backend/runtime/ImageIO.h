#pragma once

#include "IG_Config.h"

namespace IG {
/// Currently supported meta data information
struct ImageMetaData {
    std::optional<Vector3f> CameraEye;
    std::optional<Vector3f> CameraUp;
    std::optional<Vector3f> CameraDir;
    std::optional<size_t> SamplePerPixel;
    std::optional<std::string> TechniqueType;
    std::optional<std::string> CameraType;
};

class ImageIO {
public:
    static bool save(const std::filesystem::path& path, size_t width, size_t height,
                     const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names,
                     const ImageMetaData& metaData = ImageMetaData());
};
} // namespace IG
