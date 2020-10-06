#pragma once

#include "TriMesh.h"

namespace IG {
namespace ply {
TriMesh load(const std::filesystem::path& path);
}
} // namespace IG