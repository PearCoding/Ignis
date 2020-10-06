#pragma once

#include "IG_Config.h"

namespace IG {
struct Camera {
	Vector3f eye;
	Vector3f dir;
	Vector3f right;
	Vector3f up;
	float w, h;

	inline Camera(const Vector3f& e, const Vector3f& d, const Vector3f& u, float fov, float ratio)
	{
		eye	  = e;
		dir	  = d.normalized();
		right = dir.cross(u).normalized();
		up	  = right.cross(dir).normalized();

		w = std::tan(fov * Deg2Rad / 2);
		h = w / ratio;
	}

	inline void rotate(float yaw, float pitch)
	{
		dir	  = Eigen::AngleAxisf(-pitch, right) * Eigen::AngleAxisf(-yaw, up) * dir;
		right = dir.cross(up).normalized();
		up	  = right.cross(dir).normalized();
	}

	inline void roll(float angle)
	{
		right = Eigen::AngleAxisf(angle, dir) * right;
		up	  = Eigen::AngleAxisf(angle, dir) * up;
	}

	inline void update_dir(const Vector3f& ndir, const Vector3f& nup)
	{
		dir	  = ndir;
		right = dir.cross(nup).normalized();
		up	  = right.cross(dir).normalized();
	}

	inline void move(float x, float y, float z)
	{
		eye += right * x + up * y + dir * z;
	}
};
} // namespace IG