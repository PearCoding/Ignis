#pragma once

#include "IG_Config.h"

namespace IG {
struct CameraOrientation {
    Vector3f Eye = Vector3f::Zero();
    Vector3f Dir = Vector3f::UnitZ();
    Vector3f Up  = Vector3f::UnitY();
};
} // namespace IG
