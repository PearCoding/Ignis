#pragma once

#include "Parser.h"
#include "math/BoundingBox.h"

#include <unordered_set>
#include <vector>

namespace IG {

struct Shape {
	size_t VtxCount;
	size_t ItxCount;
	IG::BoundingBox BoundingBox;
};

struct Entity {
	Transformf Transform;
	uint32 Shape;
	uint32 BSDF;
};

struct LoaderEnvironment {
	std::vector<Shape> Shapes;
	std::unordered_map<std::string, uint32> EntityIDs; // TODO: This can be large... maybe change?
	std::unordered_map<std::string, uint32> ShapeIDs;
	std::unordered_map<std::string, uint32> BsdfIDs;

	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;
};

} // namespace IG