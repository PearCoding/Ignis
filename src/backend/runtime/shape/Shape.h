#pragma once

#include "math/BoundingBox.h"

namespace IG {
struct Shape {
    class ShapeProvider* Provider;
    uint32 BvhID; // Custom id used by two-level bvhs
    IG::BoundingBox BoundingBox;
};
}