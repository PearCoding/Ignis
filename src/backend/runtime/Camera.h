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

        SensorWidth  = std::tan(fov * Deg2Rad / 2);
        SensorHeight = SensorWidth / ratio;

        TMin = tmin;
        TMax = tmax;
    }

    inline void rotate(float yaw, float pitch)
    {
        update_dir(Eigen::AngleAxisf(-yaw, Up) * Eigen::AngleAxisf(-pitch, Right) * Direction, Up);
    }

    inline void rotate_fixroll(float yaw, float pitch)
    {
        Eigen::AngleAxisf pitchAngle(-pitch, Vector3f::UnitX());
        Eigen::Quaternionf q1 = Eigen::Quaternionf(pitchAngle).normalized();

        Direction = q1 * Direction;
        Right     = q1 * Right;
        Up        = Right.cross(Direction).normalized();

        Eigen::AngleAxisf yawAngle(-yaw, Up);
        Eigen::Quaternionf q2 = Eigen::Quaternionf(yawAngle).normalized();
        Direction             = q2 * Direction;
        Right                 = q2 * Right;
        Up                    = q2 * Up;
    }

    inline void roll(float angle)
    {
        Right = Eigen::AngleAxisf(angle, Direction) * Right;
        Up    = Eigen::AngleAxisf(angle, Direction) * Up;
    }

    inline void update_dir(const Vector3f& ndir, const Vector3f& nup)
    {
        Direction = ndir;
        Right     = Direction.cross(nup).normalized();
        Up        = Right.cross(Direction).normalized();
    }

    inline void move(float x, float y, float z)
    {
        Eye += Right * x + Up * y + Direction * z;
    }
};
} // namespace IG