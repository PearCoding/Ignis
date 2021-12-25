#pragma once

namespace IG {
enum class DebugMode {
    Normal = 0,
    Tangent,
    Bitangent,
    GeometryNormal,
    TexCoords,
    PrimCoords,
    Point,
    HitDistance,
    PrimIDRaw,
    PrimID,
    EntityIDRaw,
    EntityID,
    IsEmissive,
    IsSpecular,
    IsEntering,
    CheckBSDF
};
}