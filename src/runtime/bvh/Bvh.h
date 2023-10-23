#pragma once

#include "math/BoundingBox.h"

IG_BEGIN_IGNORE_WARNINGS
#include <bvh/v2/bbox.h>
#include <bvh/v2/bvh.h>
#include <bvh/v2/tri.h>
#include <bvh/v2/vec.h>
IG_END_IGNORE_WARNINGS

namespace IG {
namespace bvh {
using Vec3 = ::bvh::v2::Vec<float, 3>;
using Bbox = ::bvh::v2::BBox<float, 3>;
using Tri  = ::bvh::v2::Tri<float, 3>;
using Node = ::bvh::v2::Node<float, 3>;
using Bvh  = ::bvh::v2::Bvh<Node>;

inline Vec3 from(const Vector3f& vec) { return Vec3{ vec.x(), vec.y(), vec.z() }; }
inline Bbox from(const BoundingBox& bbox) { return Bbox{ from(bbox.min), from(bbox.max) }; }

} // namespace bvh
} // namespace IG