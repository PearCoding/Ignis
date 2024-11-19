#pragma once

#include <cstdint>
#include <cstdlib>

namespace IG {
class Runtime;

namespace ui {
/// @brief View of the runtime registry with option to view internal ones. Completly ignores user hints
bool ui_registry_view(Runtime* runtime, bool readonly = false);

/// @brief Userfriendly property viewer of the scene parameters specified by the user
bool ui_property_view(Runtime* runtime, bool readonly = false);
} // namespace ui

} // namespace IG
