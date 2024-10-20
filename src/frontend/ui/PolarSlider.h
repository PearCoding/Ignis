#pragma once

#include "IG_Config.h"
#include "UI.h"

namespace IG::ui {
bool PolarSliderFloat(const char* id, float* value, float v_min = 0, float v_max = 2 * Pi, int increments = 4);
} // namespace IG::ui