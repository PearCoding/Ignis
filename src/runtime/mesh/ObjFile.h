#pragma once

#include "TriMesh.h"

namespace IG::obj {
[[nodiscard]] TriMesh load(const std::filesystem::path& path, const std::optional<size_t>& shape_index = {});
}
