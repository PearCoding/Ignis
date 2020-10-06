#pragma once

#include "TriMesh.h"

namespace IG {
namespace obj {

TriMesh load(const std::filesystem::path& path, size_t mtl_offset);

}
} // namespace IG
