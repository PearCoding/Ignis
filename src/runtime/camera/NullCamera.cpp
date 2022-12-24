#include "NullCamera.h"

namespace IG {
NullCamera::NullCamera()
    : Camera("null")
{
}

void NullCamera::serialize(const SerializationInput& input) const
{
    input.Stream << "  let camera = make_null_camera();" << std::endl;
}

CameraOrientation NullCamera::getOrientation(const LoaderContext&) const
{
    return CameraOrientation();
}
} // namespace IG