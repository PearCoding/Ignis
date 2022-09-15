#pragma once

#include "DynTable.h"
#include "math/BoundingBox.h"

namespace IG {
struct SceneBVH {
    std::vector<uint8> Nodes;
    std::vector<uint8> Leaves;
};

struct PerPrimTypeDatabase {
    DynTable ShapeTable;
    DynTable BVHTable; // TODO: Single level bvh?
    IG::SceneBVH SceneBVH;
};

struct SceneDatabase {
    std::unordered_map<std::string_view, PerPrimTypeDatabase> PrimTypeDatabase;
    std::unordered_map<std::string, DynTable> CustomTables;

    DynTable EntityTable;

    float SceneRadius;
    BoundingBox SceneBBox;
    size_t MaterialCount;
    std::vector<uint32> EntityToMaterial; // Map from Entity -> Material (It would be better to get rid of this, but sorting by entity is "better" than sorting for material)
};
} // namespace IG