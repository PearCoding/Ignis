#pragma once

#include "DynTable.h"

namespace IG {
struct SceneDatabase {
	DynTable EntityTable;
	DynTable ShapeTable;
	DynTable LightTable;
	DynTable ShaderTable;
	DynTable BVHTable;
};
} // namespace IG