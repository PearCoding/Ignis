#pragma once

#include "DynTable.h"

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
};
} // namespace IG