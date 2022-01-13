#pragma once

#include "DynTable.h"
#include "math/BoundingBox.h"

namespace IG {
struct SceneBVH {
    std::vector<uint8> Nodes;
    std::vector<uint8> Leaves;
};

struct SceneDatabase {
    DynTable EntityTable;
    DynTable ShapeTable;
    DynTable BVHTable;

    IG::SceneBVH SceneBVH;
    float SceneRadius;
    BoundingBox SceneBBox;
};
} // namespace IG