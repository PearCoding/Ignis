#include "PerspectiveCamera.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"

namespace IG {
PerspectiveCamera::PerspectiveCamera(const Parser::Object& camera)
    : Camera("perspective")
{
    if (camera.property("transform").isValid())
        mTransform = camera.property("transform").getTransform();
    if (camera.property("aspect_ratio").isValid())
        mAspectRatio = camera.property("aspect_ratio").getNumber(1);

    mFOV = extractFOV(camera);

    mNearClip       = camera.property("near_clip").getNumber(0);
    mFarClip        = camera.property("far_clip").getNumber(std::numeric_limits<float>::max());
    mFocalLength    = camera.property("focal_length").getNumber(1);
    mApertureRadius = camera.property("aperture_radius").getNumber(0);

    if (mFarClip < mNearClip)
        std::swap(mNearClip, mFarClip);
}

void PerspectiveCamera::serialize(const SerializationInput& input) const
{
    CameraOrientation orientation = getOrientation(input.Context);

    // Dump camera control (above is just defaults)
    input.Stream << "  let camera_eye = registry::get_global_parameter_vec3(\"__camera_eye\", " << LoaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
                 << "  let camera_dir = registry::get_global_parameter_vec3(\"__camera_dir\", " << LoaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
                 << "  let camera_up  = registry::get_global_parameter_vec3(\"__camera_up\" , " << LoaderUtils::inlineVector(orientation.Up) << ");" << std::endl;

    std::string aspect_ratio = "settings.width as f32 / settings.height as f32";
    if (mAspectRatio.has_value())
        aspect_ratio = std::to_string(*mAspectRatio);

    const std::string fov_gen = mFOV.Vertical ? "compute_scale_from_vfov" : "compute_scale_from_hfov";

    if (mApertureRadius <= FltEps) {
        input.Stream << "  let camera = make_perspective_camera(camera_eye, " << std::endl
                     << "    camera_dir, " << std::endl
                     << "    camera_up, " << std::endl
                     << "    " << fov_gen << "(" << mFOV.Value << ", " << aspect_ratio << "), " << std::endl
                     << "    settings.width, " << std::endl
                     << "    settings.height, " << std::endl
                     << "    " << mNearClip << ", " << std::endl
                     << "    " << mFarClip << ");" << std::endl;
    } else {
        input.Stream << "  let camera = make_perspective_dof_camera(camera_eye, " << std::endl
                     << "    camera_dir, " << std::endl
                     << "    camera_up, " << std::endl
                     << "    " << fov_gen << "(" << mFOV.Value << ", " << aspect_ratio << "), " << std::endl
                     << "    " << mApertureRadius << ", " << std::endl
                     << "    " << mFocalLength << ", " << std::endl
                     << "    settings.width, " << std::endl
                     << "    settings.height, " << std::endl
                     << "    " << mNearClip << ", " << std::endl
                     << "    " << mFarClip << ");" << std::endl;
    }
}

CameraOrientation PerspectiveCamera::getOrientation(const LoaderContext& ctx) const
{
    CameraOrientation orientation;
    if (mTransform.has_value()) {
        const auto cameraTransform = *mTransform;
        orientation.Eye            = cameraTransform * Vector3f::Zero();
        orientation.Dir            = cameraTransform.linear().col(2);
        orientation.Up             = cameraTransform.linear().col(1);
    } else {
        const auto sceneBBox = ctx.SceneBBox;

        // Special case when the scene is completely empty
        if (sceneBBox.isEmpty()) {
            orientation.Dir = -Vector3f::UnitZ();
            orientation.Up  = Vector3f::UnitY();
            orientation.Eye = Vector3f::Zero();
            return orientation;
        }

        // Try to setup a view over the whole scene
        const float aspect_ratio = mAspectRatio.value_or(ctx.Options.FilmWidth / static_cast<float>(ctx.Options.FilmHeight));
        const float a            = sceneBBox.diameter().x() / (2 * (mFOV.Vertical ? aspect_ratio : 1)); // TODO: Really?
        const float b            = sceneBBox.diameter().y() / (2 * (!mFOV.Vertical ? aspect_ratio : 1));
        const float s            = std::sin(mFOV.Value / 2);
        const float d            = std::abs(s) <= FltEps ? 0 : std::max(a, b) * std::sqrt(1 / (s * s) - 1);

        orientation.Dir = -Vector3f::UnitZ();
        orientation.Up  = Vector3f::UnitY();
        orientation.Eye = Vector3f::UnitX() * sceneBBox.center().x()
                          + Vector3f::UnitY() * sceneBBox.center().y()
                          + Vector3f::UnitZ() * (sceneBBox.max.z() + d);
    }

    return orientation;
}
} // namespace IG