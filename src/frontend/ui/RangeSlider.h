#pragma once

#include "UI.h"

namespace IG::ui {
bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max, const char* single_display_format = "%.3f", ImGuiSliderFlags flags = 0);
}