#pragma once

#include "DynTable.h"
#include "FixTable.h"
#include "math/BoundingBox.h"

namespace IG {
struct SceneBVH {
    std::vector<uint8> Nodes;
    std::vector<uint8> Leaves;
};

struct SceneDatabase {
    std::unordered_map<std::string_view, SceneBVH> SceneBVHs;
    std::unordered_map<std::string, DynTable> DynTables;
    std::unordered_map<std::string, FixTable> FixTables;

    float SceneRadius;
    BoundingBox SceneBBox;
    size_t MaterialCount;
};
} // namespace IG