#pragma once

#include "IG_Config.h"
#include <tuple>

namespace IG {
namespace Spherical {

[[nodiscard]] inline Vector3f fromThetaPhi(float theta, float phi)
{
    const float ct = std::cos(theta);
    const float st = std::sin(theta);
    const float cp = std::cos(phi);
    const float sp = std::sin(phi);
    return Vector3f(st * cp, st * sp, ct);
}

[[nodiscard]] inline std::tuple<float, float> toThetaPhi(const Vector3f& u)
{
    const float theta = std::acos(u.z());
    const float phi   = std::atan2(u.y(), u.x());
    return { theta, phi };
}

} // namespace Spherical
} // namespace IG
