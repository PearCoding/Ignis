#pragma once

#include "IG_Config.h"

namespace IG {
    bool saveImageRGB(const std::filesystem::path& path, const float* rgb, size_t width, size_t height, float scale);
    bool saveImageRGBA(const std::filesystem::path& path, const float* rgba, size_t width, size_t height, float scale);
}