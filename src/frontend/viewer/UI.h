#pragma once

#include "Camera.h"

namespace IG {
namespace UI {
void init(int width, int height, const float* pixels);
void close();
void setTitle(const char* str);
bool handleInput(uint32_t& iter, Camera& cam);
void update(uint32_t iter);
} // namespace UI
} // namespace IG