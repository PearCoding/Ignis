#pragma once

#include "Camera.h"

namespace IG {
class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(SceneObject& obj);
    ~PerspectiveCamera() = default;

    void serialize(const SerializationInput& input) const override;
    CameraOrientation getOrientation(const LoaderContext& ctx) const override;

private:
    std::optional<Transformf> mTransform;
    std::optional<float> mAspectRatio;
    FOV mFOV;
    float mNearClip;
    float mFarClip;
    float mFocalLength;
    float mApertureRadius;
};
} // namespace IG