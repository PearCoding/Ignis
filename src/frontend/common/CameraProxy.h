#pragma once

#include "camera/CameraOrientation.h"

namespace IG {
struct CameraProxy {
    Vector3f Eye;
    Vector3f Direction;
    Vector3f Right;
    Vector3f Up;

    inline CameraProxy(const Vector3f& e, const Vector3f& d, const Vector3f& u)
        : Eye(e)
    {
        update_dir(d.normalized(), u.normalized());
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

    inline void rotate_around(const Vector3f& center, float yaw, float pitch)
    {
        // The center does not necessarily have to be on the correct direction, so select a new virtual point instead
        const Vector3f pointOnPlane = Eye - (Eye - center).dot(Direction) * Direction;
        const Quaternionf rotation  = Eigen::AngleAxisf(-yaw, Up) * Eigen::AngleAxisf(-pitch, Right);

        Eye = rotation * (Eye - pointOnPlane) + pointOnPlane;
        update_dir(rotation * Direction, rotation * Up);
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

    // Snap up such that it is in some unit direction
    inline void snap_up()
    {
        Vector3f::Index index = 0;
        Up.cwiseAbs().maxCoeff(&index);
        float sign = std::copysignf(1, Up[index]);

        switch (index) {
        case 0:
            update_dir(Direction, sign * Vector3f::UnitX());
            break;
        case 1:
            update_dir(Direction, sign * Vector3f::UnitY());
            break;
        default:
        case 2:
            update_dir(Direction, sign * Vector3f::UnitZ());
            break;
        }
    }

    inline CameraOrientation asOrientation() const
    {
        CameraOrientation orientation;
        orientation.Eye = Eye;
        orientation.Dir = Direction;
        orientation.Up  = Up;
        return orientation;
    }
};
} // namespace IG