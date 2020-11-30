#pragma once

#include "Loader.h"
#include "math/BoundingBox.h"
#include "mesh/TriMesh.h"

#include <unordered_set>
#include <vector>

namespace IG {


struct Shape {
	size_t VtxOffset;
	size_t ItxOffset;
	size_t VtxCount;
	size_t ItxCount;
};

struct Entity {
	Transformf Transform;
	std::string Shape;
	std::string BSDF;
};

struct GeneratorEnvironment {
	std::unordered_map<std::string, Shape> Shapes;
	std::unordered_map<std::string, Entity> Entities;
	TriMesh Mesh;

	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;

	void calculateBoundingBox();
};

} // namespace IG