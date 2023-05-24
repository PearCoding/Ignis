#pragma once

#include "TriMesh.h"

namespace IG::obj {
[[nodiscard]] TriMesh load(const Path& path, const std::optional<size_t>& shape_index = {});
}
