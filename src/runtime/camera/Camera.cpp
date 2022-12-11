#include "Camera.h"
#include "loader/Parser.h"

namespace IG {
Camera::FOV Camera::extractFOV(const SceneObject& camera)
{
    constexpr float DefaultFOV = 60.0f;

    if (camera.property("vfov").canBeNumber())
        return { true, camera.property("vfov").getNumber(DefaultFOV) * Deg2Rad };
    else if (camera.property("hfov").canBeNumber())
        return { false, camera.property("hfov").getNumber(DefaultFOV) * Deg2Rad };
    else
        return { false, camera.property("fov").getNumber(DefaultFOV) * Deg2Rad };
}
} // namespace IG