#pragma once

#include "TriMesh.h"

namespace IG::ply {
[[nodiscard]] TriMesh load(const std::filesystem::path& path);
bool save(const TriMesh& mesh, const std::filesystem::path& path);
}