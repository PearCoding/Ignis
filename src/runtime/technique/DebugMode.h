#pragma once

#include "IG_Config.h"

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
    GeneratedCoords,
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

[[nodiscard]] std::optional<DebugMode> stringToDebugMode(const std::string& name);
[[nodiscard]] std::string debugModeToString(DebugMode mode);

/// Will return names in order of the debug mode enum
[[nodiscard]] std::vector<std::string> getDebugModeNames();

} // namespace IG