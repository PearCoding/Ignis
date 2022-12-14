#pragma once

#include "TriMesh.h"

namespace IG::mts {
// Load mesh from Mitsuba serialized format
[[nodiscard]] TriMesh load(const std::filesystem::path& file, size_t shapeIndex = 0);
} // namespace IG::mts