#pragma once

#include "TriMesh.h"

namespace IG::ply {
[[nodiscard]] TriMesh load(const Path& path);
bool save(const TriMesh& mesh, const Path& path);
}