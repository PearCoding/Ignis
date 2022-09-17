#pragma once

#include "DynTable.h"
#include "math/BoundingBox.h"

namespace IG {
struct SceneBVH {
    std::vector<uint8> Nodes;
    std::vector<uint8> Leaves;
};

struct SceneDatabase {
    std::unordered_map<std::string_view, SceneBVH> SceneBVHs;
    std::unordered_map<std::string, DynTable> Tables;

    DynTable Shapes;
    DynTable Entities;

    float SceneRadius;
    BoundingBox SceneBBox;
    size_t MaterialCount;
    std::vector<uint32> EntityToMaterial; // Map from Entity -> Material (It would be better to get rid of this, but sorting by entity is "better" than sorting for material)
};
} // namespace IG