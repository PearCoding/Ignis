#pragma once

#include "Parser.h"
#include "math/BoundingBox.h"
#include "mesh/PlaneShape.h"

#include <unordered_set>
#include <vector>

namespace IG {

struct Shape {
    size_t VertexCount;
    size_t NormalCount;
    size_t TexCount;
    size_t FaceCount;
    IG::BoundingBox BoundingBox;
    float Area;
};

struct Entity {
    Transformf Transform;
    std::string Name;
    std::string Shape;
    std::string BSDF;

    inline Eigen::Matrix<float, 3, 4> computeLocalMatrix() const
    {
        return Transform.inverse().matrix().block<3, 4>(0, 0);
    }
    inline Eigen::Matrix<float, 3, 4> computeGlobalMatrix() const
    {
        return Transform.matrix().block<3, 4>(0, 0);
    }
    inline Eigen::Matrix<float, 3, 3> computeGlobalNormalMatrix() const
    {
        return Transform.matrix().block<3, 3>(0, 0).inverse().transpose();
    }
};

/// A material is a combination of bsdf, entity (if the entity is emissive) and volume/medium interface
struct Material {
    std::string BSDF;
    int MediumInner = -1;
    int MediumOuter = -1;
    std::string Entity; // Empty if not emissive
    inline bool hasEmission() const { return !Entity.empty(); }
    inline bool hasMediumInterface() const { return MediumInner >= 0 || MediumOuter >= 0; }
};

inline bool operator==(const Material& a, const Material& b)
{
    return a.BSDF == b.BSDF && a.MediumInner == b.MediumInner && a.MediumOuter == b.MediumOuter && a.Entity == b.Entity;
}

// TODO: Refactor this to the actual loading classes
struct LoaderEnvironment {
    std::vector<Shape> Shapes;
    std::unordered_map<std::string, uint32> ShapeIDs;
    std::unordered_map<std::string, Entity> EmissiveEntities;
    std::vector<Material> Materials;
    std::unordered_map<uint32, PlaneShape> PlaneShapes; // Used for special optimization

    BoundingBox SceneBBox;
    float SceneDiameter = 0.0f;
};

} // namespace IG