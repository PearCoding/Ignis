#pragma once

#include "IG_Config.h"

namespace IG {
// Default position getter
template <typename T>
struct DefaultPositionGetter;

// Simple position getter
template <>
struct DefaultPositionGetter<Vector3f> {
    inline Vector3f operator()(const Vector3f& p) const
    {
        return p;
    }
};

} // namespace IG
