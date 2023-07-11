#pragma once

#include "UIGlue.h"

namespace IGGui {
bool RangeSliderFloat(const char* label, float* v1, float* v2, float v_min, float v_max, const char* single_display_format, ImGuiSliderFlags flags);
}