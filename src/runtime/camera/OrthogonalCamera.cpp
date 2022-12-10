#include "OrthogonalCamera.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"

namespace IG {
OrthogonalCamera::OrthogonalCamera(const Parser::Object& camera)
    : Camera("orthogonal")
{
    mTransform = camera.property("transform").getTransform();
    if (camera.property("aspect_ratio").isValid())
        mAspectRatio = camera.property("aspect_ratio").getNumber(1);

    mNearClip = camera.property("near_clip").getNumber(0);
    mFarClip  = camera.property("far_clip").getNumber(std::numeric_limits<float>::max());
    mScale    = camera.property("scale").getNumber(1);

    if (mFarClip < mNearClip)
        std::swap(mNearClip, mFarClip);
}

void OrthogonalCamera::serialize(const SerializationInput& input) const
{
    CameraOrientation orientation = getOrientation(input.Context);

    std::string aspect_ratio = "settings.width as f32 / settings.height as f32";
    if (mAspectRatio.has_value())
        aspect_ratio = std::to_string(*mAspectRatio);

    // Dump camera control (above is just defaults)
    input.Stream << "  let camera_eye   = registry::get_global_parameter_vec3(\"__camera_eye\", " << LoaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
                 << "  let camera_dir   = registry::get_global_parameter_vec3(\"__camera_dir\", " << LoaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
                 << "  let camera_up    = registry::get_global_parameter_vec3(\"__camera_up\" , " << LoaderUtils::inlineVector(orientation.Up) << ");" << std::endl
                 << "  let camera_scale = registry::get_global_parameter_f32(\"__camera_scale\" , 1);" << std::endl
                 << "  let camera = make_orthogonal_camera(camera_eye, " << std::endl
                 << "    camera_dir, " << std::endl
                 << "    camera_up, " << std::endl
                 << "    make_vec2(" << mScale << " * camera_scale, " << mScale << " * camera_scale / " << aspect_ratio << "), " << std::endl
                 << "    settings.width, " << std::endl
                 << "    settings.height, " << std::endl
                 << "    " << mNearClip << ", " << std::endl
                 << "    " << mFarClip << ");" << std::endl;
}

CameraOrientation OrthogonalCamera::getOrientation(const LoaderContext&) const
{
    CameraOrientation orientation;
    orientation.Eye = mTransform * Vector3f::Zero();
    orientation.Dir = mTransform.linear().col(2);
    orientation.Up  = mTransform.linear().col(1);
    // TODO: Maybe add a scene dependent orientation?
    return orientation;
}
} // namespace IG