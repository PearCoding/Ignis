#pragma once

#include "IG_Config.h"

namespace IG {
/// Bounding box represented by its two extreme points.
struct BoundingBox {
	Vector3f min, max;

	inline BoundingBox() {}
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
		const float kx	   = std::max(len(0), 0.0f);
		const float ky	   = std::max(len(1), 0.0f);
		const float kz	   = std::max(len(2), 0.0f);
		return kx * (ky + kz) + ky * kz;
	}

	inline BoundingBox& overlap(const BoundingBox& bb)
	{
		min = min.cwiseMax(bb.min);
		max = max.cwiseMin(bb.max);
		return *this;
	}

	inline bool isEmpty() const
	{
		return min.x > max.x || min.y > max.y || min.z > max.z;
	}

	inline bool isInside(const Vector3f& v) const
	{
		return (v >= min).all() && (v <= max).all();
	}

	inline bool isOverlapping(const BoundingBox& bb) const
	{
		return (min <= bb.max).all() && (max >= bb.min).all();
	}

	inline static BoundingBox Empty() { return BoundingBox(Vector3f(FLT_MAX), Vector3f(-FLT_MAX)); }
	inline static BoundingBox Full() { return BoundingBox(Vector3f(-FLT_MAX), Vector3f(FLT_MAX)); }
};

} // namespace IG