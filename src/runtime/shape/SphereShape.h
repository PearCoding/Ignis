#pragma once

#include "IG_Config.h"

namespace IG {
struct SphereShape {
    Vector3f Origin;
    float Radius;

    [[nodiscard]] inline float computeArea() const { return 4 * Pi * Radius * Radius; }
};
}