#pragma once

#include "math/BoundingBox.h"
#include "mesh/TriMesh.h"

#include "tinyparser-mitsuba.h"

#include <unordered_set>
#include <vector>

namespace IG {

using TPMObject = TPM_NAMESPACE::Object;

struct Material {
	uint32_t MeshLightPairId = 0;
	std::shared_ptr<TPMObject> BSDF;
	std::shared_ptr<TPMObject> Light;
};

inline bool operator==(const Material& a, const Material& b)
{
	return a.BSDF == b.BSDF && a.Light == b.Light && (!a.Light || a.MeshLightPairId == b.MeshLightPairId);
}

class MaterialHash {
public:
	size_t operator()(const Material& s) const
	{
		size_t h1 = std::hash<decltype(s.BSDF)>()(s.BSDF);
		size_t h2 = std::hash<decltype(s.Light)>()(s.Light);
		size_t h3 = std::hash<decltype(s.MeshLightPairId)>()(s.Light ? s.MeshLightPairId : 0); // Dont bother with meshid if no light is present
		return (h1 ^ (h2 << 1)) ^ (h3 << 1);
	}
};

struct Shape {
	size_t VtxOffset;
	size_t ItxOffset;
	size_t VtxCount;
	size_t ItxCount;
	IG::Material Material;
};

struct GeneratorEnvironment {
	std::vector<Shape> Shapes;
	std::vector<Material> Materials;
	std::unordered_set<std::shared_ptr<TPMObject>> Textures;
	TriMesh Mesh;
	
	BoundingBox SceneBBox;
	float SceneDiameter = 0.0f;

	void calculateBoundingBox();
};

} // namespace IG