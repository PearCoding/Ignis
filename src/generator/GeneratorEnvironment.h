#pragma once

#include "Loader.h"
#include "math/BoundingBox.h"

#include <unordered_set>
#include <vector>

namespace IG {

struct Shape {
	size_t VtxCount;
	size_t ItxCount;
	IG::BoundingBox BoundingBox;
	std::string TriMesh;
	std::string BVH;
};

struct Entity {
	Transformf Transform;
	std::string Shape;
	std::string BSDF;
};

struct GeneratorEnvironment {
	std::unordered_map<std::string, Shape> Shapes;
	std::unordered_map<std::string, Entity> Entities;

	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;
};

} // namespace IG