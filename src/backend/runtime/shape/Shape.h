#pragma once

#include "math/BoundingBox.h"

namespace IG {
struct Shape {
    class ShapeProvider* Provider;
    IG::BoundingBox BoundingBox;
};
}