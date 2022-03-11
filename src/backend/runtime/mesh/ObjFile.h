#pragma once

#include "TriMesh.h"

namespace IG::obj {
TriMesh load(const std::filesystem::path& path);
}
