#pragma once

#include "TriMesh.h"

namespace IG::ply {
[[nodiscard]] TriMesh load(const std::filesystem::path& path);
}