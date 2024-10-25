#pragma once

#include "IG_Config.h"

namespace IG {
bool saveImageRGB(const Path& path, const float* rgb, size_t width, size_t height, float scale);
bool saveImageRGBA(const Path& path, const float* rgba, size_t width, size_t height, float scale);
} // namespace IG