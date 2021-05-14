#pragma once

#include "TriMesh.h"

namespace IG {
namespace mts {
// Load mesh from Mitsuba serialized format
TriMesh load(const std::filesystem::path& file, size_t shapeIndex = 0);
} // namespace mts
} // namespace IG