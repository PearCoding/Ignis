#pragma once

#include "IG_Config.h"

namespace IG {
/// Bounding box represented by its two extreme points.
struct BoundingBox {
    Vector3f min = Vector3f::Zero();
    Vector3f max = Vector3f::Zero();

    inline BoundingBox() = default;
    inline BoundingBox(const Vector3f& f)
        : min(f)
        , max(f)
    {
    }

    inline BoundingBox(const Vector3f& min, const Vector3f& max)
        : min(min)
        , max(max)
    {
    }

    inline Vector3f diameter() const { return max - min; }

    inline BoundingBox& extend(const BoundingBox& bb)
    {
        min = min.cwiseMin(bb.min);
        max = max.cwiseMax(bb.max);
        return *this;
    }

    inline BoundingBox& extend(const Vector3f& v)
    {
        min = min.cwiseMin(v);
        max = max.cwiseMax(v);
        return *this;
    }

    inline float halfArea() const
    {
        const Vector3f len = max - min;
        const float kx     = std::max(len(0), 0.0f);
        const float ky     = std::max(len(1), 0.0f);
        const float kz     = std::max(len(2), 0.0f);
        return kx * (ky + kz) + ky * kz;
    }

    inline float volume() const
    {
        const Vector3f len = max - min;
        return len(0) * len(1) * len(2);
    }

    inline void inflate(float eps)
    {
        const Vector3f len = max - min;
        for (size_t i = 0; i < 3; ++i) {
            if (len(i) < eps) {
                max[i] += eps / 2;
                min[i] -= eps / 2;
            }
        }
    }

    inline BoundingBox& overlap(const BoundingBox& bb)
    {
        min = min.cwiseMax(bb.min);
        max = max.cwiseMin(bb.max);
        return *this;
    }

    inline void computeSplit(BoundingBox& left_bb, BoundingBox& right_bb, int axis, float split) const
    {
        left_bb  = *this;
        right_bb = *this;

        const float off = (max(axis) - min(axis)) * split;
        left_bb.max(axis) -= off;
        right_bb.min(axis) += off;
    }

    inline BoundingBox transformed(const Transformf& transform) const
    {
        BoundingBox bbox = Empty();
        bbox.extend(transform * min);                              // 000
        bbox.extend(transform * Vector3f(max(0), min(1), min(2))); // 100
        bbox.extend(transform * Vector3f(min(0), max(1), min(2))); // 010
        bbox.extend(transform * Vector3f(max(0), max(1), min(2))); // 110
        bbox.extend(transform * Vector3f(min(0), min(1), max(2))); // 001
        bbox.extend(transform * Vector3f(max(0), min(1), max(2))); // 101
        bbox.extend(transform * Vector3f(min(0), max(1), max(2))); // 011
        bbox.extend(transform * max);                              // 111
        return bbox;
        /*const Vector3f xmin = transform.linear().col(0) * min(0);
		const Vector3f xmax = transform.linear().col(0) * max(0);

		const Vector3f ymin = transform.linear().col(1) * min(1);
		const Vector3f ymax = transform.linear().col(1) * max(1);

		const Vector3f zmin = transform.linear().col(2) * min(2);
		const Vector3f zmax = transform.linear().col(2) * max(2);

		return BoundingBox(
			xmin.cwiseMin(xmax) + ymin.cwiseMin(ymax) + zmin.cwiseMin(zmax) + transform.translation(),
			xmin.cwiseMax(xmax) + ymin.cwiseMax(ymax) + zmin.cwiseMax(zmax) + transform.translation());*/
    }

    inline bool isEmpty() const
    {
        return (min.array() > max.array()).any();
    }

    inline bool isInside(const Vector3f& v) const
    {
        return (v.array() >= min.array()).all() && (v.array() <= max.array()).all();
    }

    inline bool isOverlapping(const BoundingBox& bb) const
    {
        return (min.array() <= bb.max.array()).all() && (max.array() >= bb.min.array()).all();
    }

    inline static BoundingBox Empty() { return BoundingBox(Vector3f(FltMax, FltMax, FltMax), Vector3f(-FltMax, -FltMax, -FltMax)); }
    inline static BoundingBox Full() { return BoundingBox(Vector3f(-FltMax, -FltMax, -FltMax), Vector3f(FltMax, FltMax, FltMax)); }
};

} // namespace IG