#pragma once

#include "BoundingBox.h"

namespace IG {
struct Triangle {
    Vector3f v0, v1, v2;

    inline Triangle() {}
    inline Triangle(const Vector3f& v0, const Vector3f& v1, const Vector3f& v2)
        : v0(v0)
        , v1(v1)
        , v2(v2)
    {
    }

    [[nodiscard]] inline Vector3f& operator[](int i) { return i == 0 ? v0 : (i == 1 ? v1 : v2); }
    [[nodiscard]] inline const Vector3f& operator[](int i) const { return i == 0 ? v0 : (i == 1 ? v1 : v2); }

    [[nodiscard]] inline float area() const { return (v1 - v0).cross(v2 - v0).norm() / 2; }

    /// Computes the triangle bounding box.
    [[nodiscard]] inline BoundingBox computeBoundingBox() const
    {
        return BoundingBox{ v0.cwiseMin(v1.cwiseMin(v2)),
                            v0.cwiseMax(v1.cwiseMax(v2)) };
    }

    /// Splits the triangle along one axis and returns the resulting two bounding boxes.
    inline void computeSplit(BoundingBox& left_bb, BoundingBox& right_bb, int axis, float split) const
    {
        left_bb  = BoundingBox::Empty();
        right_bb = BoundingBox::Empty();

        const Vector3f e0 = v1 - v0;
        const Vector3f e1 = v2 - v1;
        const Vector3f e2 = v0 - v2;

        const bool left0 = v0[axis] <= split;
        const bool left1 = v1[axis] <= split;
        const bool left2 = v2[axis] <= split;

        if (left0)
            left_bb.extend(v0);
        if (left1)
            left_bb.extend(v1);
        if (left2)
            left_bb.extend(v2);

        if (!left0)
            right_bb.extend(v0);
        if (!left1)
            right_bb.extend(v1);
        if (!left2)
            right_bb.extend(v2);

        if (left0 ^ left1) {
            const Vector3f p = clipEdge(axis, split, v0, e0);
            left_bb.extend(p);
            right_bb.extend(p);
        }
        if (left1 ^ left2) {
            const Vector3f p = clipEdge(axis, split, v1, e1);
            left_bb.extend(p);
            right_bb.extend(p);
        }
        if (left2 ^ left0) {
            const Vector3f p = clipEdge(axis, split, v2, e2);
            left_bb.extend(p);
            right_bb.extend(p);
        }
    }

private:
    inline static Vector3f clipEdge(int axis, float plane, const Vector3f& p, const Vector3f& edge)
    {
        const float t = (plane - p[axis]) / (edge[axis]);
        return p + t * edge;
    }
};

} // namespace IG