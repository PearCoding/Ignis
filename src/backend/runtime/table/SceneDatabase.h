#pragma once

#include "DynTable.h"

namespace IG {
struct SceneBVH {
	std::vector<uint8> Nodes;
	std::vector<uint8> Leaves;
};

struct SceneDatabase {
	std::vector<std::vector<uint8>> Buffers;
	DynTable TextureTable;
	DynTable EntityTable;
	DynTable ShapeTable;
	DynTable LightTable;
	DynTable BsdfTable;
	DynTable BVHTable;

	IG::SceneBVH SceneBVH;
	float SceneRadius;
};
} // namespace IG