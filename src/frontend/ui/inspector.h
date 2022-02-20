#pragma once

#include <cstdint>
#include <cstdlib>

namespace IG {
void ui_inspect_image(int px, int py, size_t width, size_t height, const float* values, const uint32_t* rgb);
} // namespace IG
