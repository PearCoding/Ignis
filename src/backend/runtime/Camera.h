#pragma once

#include "IG_Config.h"

namespace IG {
struct Camera {
	Vector3f Eye;
	Vector3f Direction;
	Vector3f Right;
	Vector3f Up;
	float SensorWidth, SensorHeight;
	float TMin, TMax;

	inline Camera(const Vector3f& e, const Vector3f& d, const Vector3f& u, float fov, float ratio, float tmin, float tmax)
	{
		Eye		  = e;
		Direction = d.normalized();
		Right	  = Direction.cross(u).normalized();
		Up		  = Right.cross(Direction).normalized();

		SensorWidth	 = std::tan(fov * Deg2Rad / 2);
		SensorHeight = SensorWidth / ratio;

		TMin = tmin;
		TMax = tmax;
	}

	inline void rotate(float yaw, float pitch)
	{
		Direction = Eigen::AngleAxisf(-pitch, Right) * Eigen::AngleAxisf(-yaw, Up) * Direction;
		Right	  = Direction.cross(Up).normalized();
		Up		  = Right.cross(Direction).normalized();
	}

	inline void roll(float angle)
	{
		Right = Eigen::AngleAxisf(angle, Direction) * Right;
		Up	  = Eigen::AngleAxisf(angle, Direction) * Up;
	}

	inline void update_dir(const Vector3f& ndir, const Vector3f& nup)
	{
		Direction = ndir;
		Right	  = Direction.cross(nup).normalized();
		Up		  = Right.cross(Direction).normalized();
	}

	inline void move(float x, float y, float z)
	{
		Eye += Right * x + Up * y + Direction * z;
	}
};
} // namespace IG