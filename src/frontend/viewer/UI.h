#pragma once

#include "Camera.h"
#include "DebugMode.h"

namespace IG {
namespace UI {
bool init(int width, int height, const float* pixels, bool showDebug);
void close();
void setTitle(const char* str);
bool handleInput(uint32_t& iter, bool& run, Camera& cam);
void update(uint32_t iter, uint32_t samplesPerIteration);
DebugMode currentDebugMode();
} // namespace UI
} // namespace IG