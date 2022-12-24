#pragma once

#include "IG_Config.h"

namespace IG {
struct PlaneShape {
    Vector3f Origin;
    Vector3f XAxis;
    Vector3f YAxis;
    std::array<Vector2f, 4> TexCoords;

    [[nodiscard]] inline Vector3f computeNormal() const { return XAxis.cross(YAxis); }
    [[nodiscard]] inline float computeArea() const { return XAxis.cross(YAxis).norm(); }
};
}