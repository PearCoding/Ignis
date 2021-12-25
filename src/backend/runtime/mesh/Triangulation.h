#pragma once

#include "IG_Config.h"

namespace IG {
class Triangulation {
public:
    /// Triangulate a three dimensional polygon by projecting to a normal and return a list of indices
    static std::vector<int> triangulate(const std::vector<Vector3f>& vertices);

    /// Triangulate a two dimensional polygon and return a list of indices
    static std::vector<int> triangulate(const std::vector<Vector2f>& vertices);
};
} // namespace IG