#pragma once

#include "IG_Config.h"

namespace IG {
constexpr float ELEVATION_RANGE = Pi2;
constexpr float AZIMUTH_RANGE   = 2 * Pi;
// Up is Y+
struct ElevationAzimuth {
    float Elevation; // [0, pi/2] - Degrees (in radians) above the horizon
    float Azimuth;   // [0, 2pi]  - Degrees (in radians) west of south

    [[nodiscard]] inline static ElevationAzimuth fromDirection(const Vector3f& direction)
    {
        const float theta_dir = std::acos(direction(1)); // Y
        const float phi_dir   = std::atan2(direction(0), -direction(2));
        return ElevationAzimuth{ Pi2 - theta_dir, phi_dir < 0 ? (phi_dir + 2 * Pi) : phi_dir };
    }

    [[nodiscard]] inline Vector3f toDirection() const
    {
        const float sinE = std::sin(Elevation);
        const float cosE = std::cos(Elevation);
        const float sinA = std::sin(Azimuth);
        const float cosA = std::cos(Azimuth);

        return Vector3f(cosE * sinA, sinE, -cosE * cosA);
    }
};
} // namespace IG