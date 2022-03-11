#pragma once

#include "IG_Config.h"

namespace IG {
constexpr float ELEVATION_RANGE = Pi2;
constexpr float AZIMUTH_RANGE   = 2 * Pi;
// Up is Y+
struct ElevationAzimuth {
    float Elevation; // [0, pi/2]
    float Azimuth;   // [0, 2pi]

    inline float theta() const { return Pi2 - Elevation; }
    inline float phi() const { return Azimuth; }

    // Based on https://www.mathworks.com/help/phased/ref/azel2phitheta.html (alternate theta/phi definition)
    inline static ElevationAzimuth fromThetaPhi(float theta, float phi)
    {
        auto ea = ElevationAzimuth{ Pi2 - theta, phi };
        if (ea.Azimuth < 0)
            ea.Azimuth += 2 * Pi;
        return ea;
    }

    inline static ElevationAzimuth fromDirection(const Vector3f& direction)
    {
        const float theta_dir = std::acos(direction(1)); // Y
        const float phi_dir   = std::atan2(direction(2), direction(0));
        return fromThetaPhi(theta_dir, phi_dir);
    }

    inline Vector2f toThetaPhi() const
    {
        return Vector2f(theta(), phi());
    }

    inline Vector3f toDirection() const
    {
        const Vector2f tp = toThetaPhi();
        const float sinT  = std::sin(tp(0));
        const float cosT  = std::cos(tp(0));

        return Vector3f(sinT * std::cos(tp(1)), cosT, sinT * std::sin(tp(1)));
    }
};
} // namespace IG