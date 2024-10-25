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

class IG_LIB ImageIO {
public:
    static bool save(const Path& path, size_t width, size_t height,
                     const std::vector<const float*>& layer_ptrs, const std::vector<std::string>& layer_names,
                     const ImageMetaData& metaData = ImageMetaData());
};
} // namespace IG
