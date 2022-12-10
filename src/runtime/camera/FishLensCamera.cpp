#include "FishLensCamera.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"

namespace IG {
FishLensCamera::FishLensCamera(const Parser::Object& camera)
    : Camera("fishlens")
    , mMode(Mode::Circular)
{
    mTransform = camera.property("transform").getTransform();

    mNearClip = camera.property("near_clip").getNumber(0);
    mFarClip  = camera.property("far_clip").getNumber(std::numeric_limits<float>::max());

    if (mFarClip < mNearClip)
        std::swap(mNearClip, mFarClip);
}

void FishLensCamera::serialize(const SerializationInput& input) const
{
    CameraOrientation orientation = getOrientation(input.Context);

    std::string mode;
    switch (mMode) {
    default:
    case Mode::Circular:
        mode = "FisheyeAspectMode::Circular";
        break;

    case Mode::Cropped:
        mode = "FisheyeAspectMode::Cropped";
        break;
    case Mode::Full:
        mode = "FisheyeAspectMode::Full";
        break;
    }

    input.Stream << "  let camera_eye = registry::get_global_parameter_vec3(\"__camera_eye\", " << LoaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
                 << "  let camera_dir = registry::get_global_parameter_vec3(\"__camera_dir\", " << LoaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
                 << "  let camera_up  = registry::get_global_parameter_vec3(\"__camera_up\" , " << LoaderUtils::inlineVector(orientation.Up) << ");" << std::endl
                 << "  let camera = make_fishlens_camera(camera_eye, " << std::endl
                 << "    camera_dir, " << std::endl
                 << "    camera_up, " << std::endl
                 << "    settings.width as f32, " << std::endl
                 << "    settings.height as f32, " << std::endl
                 << "    " << mode << ", " << std::endl
                 << "    " << mNearClip << ", " << std::endl
                 << "    " << mFarClip << ");" << std::endl;
}

CameraOrientation FishLensCamera::getOrientation(const LoaderContext&) const
{
    CameraOrientation orientation;
    orientation.Eye = mTransform * Vector3f::Zero();
    orientation.Dir = mTransform.linear().col(2);
    orientation.Up  = mTransform.linear().col(1);
    // TODO: Maybe add a scene dependent orientation?
    return orientation;
}
} // namespace IG