#pragma once

#include "IG_Config.h"

namespace IG {
bool saveImageRGB(const Path& path, const float* rgb, size_t width, size_t height, float scale);
bool saveImageRGBA(const Path& path, const float* rgba, size_t width, size_t height, float scale);

struct CameraOrientation;
class Runtime;
bool saveImageOutput(const Path& path, const Runtime& runtime, const CameraOrientation* currentOrientation);
} // namespace IG