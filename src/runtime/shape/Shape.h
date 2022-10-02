#pragma once

#include "math/BoundingBox.h"

namespace IG {
struct Shape {
    class ShapeProvider* Provider;
    int32 User1ID; // Custom id used by two-level bvhs
    int32 User2ID;
    int32 User3ID;
    IG::BoundingBox BoundingBox;
    size_t TableOffset;
};
} // namespace IG