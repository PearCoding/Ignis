#include "CameraCheckTechnique.h"

namespace IG {
CameraCheckTechnique::CameraCheckTechnique()
    : Technique("cameracheck")
{
}

TechniqueInfo CameraCheckTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].RequiresExplicitCamera = true; // We make use of the camera differential!
    return info;
}

void CameraCheckTechnique::generateBody(const SerializationInput& input) const
{
    // `camera` was defined by RequiresExplicitCamera flag
    input.Stream << "  let technique = make_camera_check_renderer(camera);" << std::endl;
}

} // namespace IG