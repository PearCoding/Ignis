#pragma once

#include <cstdint>
#include <cstdlib>

namespace IG {
class Runtime;

namespace ui {
bool ui_property_view(Runtime* runtime, bool readonly = false);
} // namespace ui

} // namespace IG
