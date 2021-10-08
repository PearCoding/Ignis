#pragma once

#include "Parser.h"
#include "math/BoundingBox.h"

#include <unordered_set>
#include <vector>

namespace IG {

struct Shape {
	size_t VertexCount;
	size_t NormalCount;
	size_t TexCount;
	size_t FaceCount;
	IG::BoundingBox BoundingBox;
};

struct Entity {
	Transformf Transform;
	std::string Name;
	std::string Shape;
	std::string BSDF;
};

struct LoaderEnvironment {
	std::vector<Shape> Shapes;
	std::vector<Entity> Entities;					   // TODO: This can be large... maybe change?
	std::unordered_map<std::string, uint32> EntityIDs; // TODO: This can be large... maybe change?
	std::unordered_map<std::string, uint32> ShapeIDs;
	std::unordered_map<std::string, std::string> AreaLightsMap; // Map from Entity -> Light

	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;
};

} // namespace IG