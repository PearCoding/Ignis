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

/// A material is a combination of bsdf & entity (if the entity is emissive)
struct Material {
    std::string BSDF;
    std::string Entity; // Empty if not emissive
    inline bool hasEmission() const { return !Entity.empty(); }
};

inline bool operator==(const Material& a, const Material& b)
{
    return a.BSDF == b.BSDF && a.Entity == b.Entity;
}

struct LoaderEnvironment {
    std::vector<Shape> Shapes;
    std::unordered_map<std::string, uint32> ShapeIDs;
    std::unordered_map<std::string, Entity> EmissiveEntities;
    std::unordered_map<std::string, std::string> AreaLightsMap; // Map from Entity -> Light
    std::vector<Material> Materials;

    BoundingBox SceneBBox;
    float SceneDiameter = 0.0f;
};

} // namespace IG