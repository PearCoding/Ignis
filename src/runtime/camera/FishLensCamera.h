#pragma once

#include "Camera.h"

namespace IG {
class FishLensCamera : public Camera {
public:
    FishLensCamera(SceneObject& obj);
    ~FishLensCamera() = default;

    void serialize(const SerializationInput& input) const override;
    CameraOrientation getOrientation(const LoaderContext& ctx) const override;

private:
    enum class Mode {
        Circular,
        Cropped,
        Full
    };

    Mode mMode;
    Transformf mTransform;
    float mNearClip;
    float mFarClip;
    bool mMask;
};
} // namespace IG