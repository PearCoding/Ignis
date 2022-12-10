#pragma once

#include "Camera.h"

namespace IG {
class OrthogonalCamera : public Camera {
public:
    OrthogonalCamera(const Parser::Object& obj);
    ~OrthogonalCamera() = default;

    void serialize(const SerializationInput& input) const override;
    CameraOrientation getOrientation(const LoaderContext& ctx) const override;

private:
    Transformf mTransform;
    std::optional<float> mAspectRatio;
    float mScale;
    float mNearClip;
    float mFarClip;
};
} // namespace IG