#pragma once

namespace IG {
enum class DebugMode {
    Normal = 0,
    Tangent,
    Bitangent,
    GeometryNormal,
    LocalNormal,
    LocalTangent,
    LocalBitangent,
    LocalGeometryNormal,
    TexCoords,
    PrimCoords,
    Point,
    LocalPoint,
    HitDistance,
    Area,
    PrimIDRaw,
    PrimID,
    EntityIDRaw,
    EntityID,
    MaterialIDRaw,
    MaterialID,
    IsEmissive,
    IsSpecular,
    IsEntering,
    CheckBSDF,
    Albedo,
    MediumInner,
    MediumOuter
};
}