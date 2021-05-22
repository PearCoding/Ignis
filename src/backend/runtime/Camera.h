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
		Eye = e;
		update_dir(d.normalized(), u.normalized());

		SensorWidth	 = std::tan(fov * Deg2Rad / 2);
		SensorHeight = SensorWidth / ratio;

		TMin = tmin;
		TMax = tmax;
	}

	inline void rotate(float yaw, float pitch)
	{
		Eigen::AngleAxisf pitchAngle(-pitch, Vector3f::UnitX());

		Direction = pitchAngle * Direction;
		Right	  = pitchAngle * Right;
		Up		  = pitchAngle * Up;

		Direction = Eigen::AngleAxisf(-yaw, Up) * Direction;
		update_dir(Direction, Up);
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