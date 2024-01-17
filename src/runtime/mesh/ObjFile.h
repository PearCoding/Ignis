#pragma once

#include "TriMesh.h"

namespace IG::obj {
/// @brief Load mesh from obj file, optionaly only a single shape
/// @param path A full path to the obj file
/// @param shape_index Optional index to load a single shape, else all shapes are merged together
/// @return 
[[nodiscard]] IG_LIB TriMesh load(const Path& path, const std::optional<size_t>& shape_index = {});

/// @brief Save a mesh as an obj file. It is recommended to use ply instead
/// @param mesh A non-empty mesh
/// @param path A path to a writable location
/// @return True if successful
IG_LIB bool save(const TriMesh& mesh, const Path& path);
}
