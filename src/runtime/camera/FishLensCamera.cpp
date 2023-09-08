#include "FishLensCamera.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"

namespace IG {
FishLensCamera::FishLensCamera(SceneObject& camera)
    : Camera("fishlens")
    , mMode(Mode::Circular)
{
    mTransform = camera.property("transform").getTransform();

    mNearClip = camera.property("near_clip").getNumber(0);
    mFarClip  = camera.property("far_clip").getNumber(std::numeric_limits<float>::max());

    if (mFarClip < mNearClip)
        std::swap(mNearClip, mFarClip);

    const std::string mode = camera.property("mode").getString("circular");
    if (mode == "cropped")
        mMode = Mode::Cropped;
    else if (mode == "full")
        mMode = Mode::Full;
    else
        mMode = Mode::Circular;
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

    // The following variables are modified by `igview` to allow interactive control
    input.Context.GlobalRegistry.VectorParameters["__camera_eye"] = orientation.Eye;
    input.Context.GlobalRegistry.VectorParameters["__camera_dir"] = orientation.Dir;
    input.Context.GlobalRegistry.VectorParameters["__camera_up"]  = orientation.Up;

    input.Stream << "  let camera_eye = registry::get_global_parameter_vec3(\"__camera_eye\", vec3_expand(0));" << std::endl
                 << "  let camera_dir = registry::get_global_parameter_vec3(\"__camera_dir\", vec3_expand(0));" << std::endl
                 << "  let camera_up  = registry::get_global_parameter_vec3(\"__camera_up\" , vec3_expand(0));" << std::endl
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