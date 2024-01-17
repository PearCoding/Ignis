#pragma once

#include "TriMesh.h"

namespace IG::ply {
[[nodiscard]] IG_LIB TriMesh load(const Path& path);
IG_LIB bool save(const TriMesh& mesh, const Path& path);
}