#pragma once

#include "Camera.h"

namespace IG {
/// @brief Special camera used in the trace environment
class NullCamera : public Camera {
public:
    NullCamera();
    ~NullCamera() = default;

    void serialize(const SerializationInput& input) const override;
    CameraOrientation getOrientation(const LoaderContext& ctx) const override;
};
} // namespace IG