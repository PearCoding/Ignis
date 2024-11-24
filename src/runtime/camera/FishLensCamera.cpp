#include "FishLensCamera.h"
#include "StringUtils.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
FishLensCamera::FishLensCamera(const std::shared_ptr<SceneObject>& camera)
    : Camera("fishlens")
    , mMode(Mode::Circular)
{
    if (camera->property("transform").isValid())
        mTransform = camera->property("transform").getTransform();

    mNearClip = camera->property("near_clip").getNumber(0);
    mFarClip  = camera->property("far_clip").getNumber(std::numeric_limits<float>::max());
    mMask     = camera->property("mask").getBool(false);

    if (mFarClip < mNearClip)
        std::swap(mNearClip, mFarClip);

    const std::string mode = to_lowercase(camera->property("mode").getString("circular"));
    if (mode == "cropped")
        mMode = Mode::Cropped;
    else if (mode == "full")
        mMode = Mode::Full;
    else
        mMode = Mode::Circular;
}

void FishLensCamera::serialize(const SerializationInput& input) const
{
    CameraOrientation orientation = getOrientation(input.Tree.context());

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
    input.Tree.context().GlobalRegistry.VectorParameters["__camera_eye"] = orientation.Eye;
    input.Tree.context().GlobalRegistry.VectorParameters["__camera_dir"] = orientation.Dir;
    input.Tree.context().GlobalRegistry.VectorParameters["__camera_up"]  = orientation.Up;

    input.Stream << "  let camera_eye = registry::get_global_parameter_vec3(\"__camera_eye\", vec3_expand(0));" << std::endl
                 << "  let camera_dir = registry::get_global_parameter_vec3(\"__camera_dir\", vec3_expand(0));" << std::endl
                 << "  let camera_up  = registry::get_global_parameter_vec3(\"__camera_up\" , vec3_expand(0));" << std::endl
                 << "  let camera = make_fishlens_camera(camera_eye, " << std::endl
                 << "    camera_dir, " << std::endl
                 << "    camera_up, " << std::endl
                 << "    settings.width, " << std::endl
                 << "    settings.height, " << std::endl
                 << "    " << mode << ", " << std::endl
                 << "    " << mNearClip << ", " << std::endl
                 << "    " << mFarClip << ", " << std::endl
                 << "    " << (mMask ? "true" : "false") << ");" << std::endl;
}

CameraOrientation FishLensCamera::getOrientation(const LoaderContext& ctx) const
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
        const float a = sceneBBox.diameter().x() / 2;
        const float b = sceneBBox.diameter().y() / 2;
        const float s = std::sin(Deg2Rad * 60.0f / 2);
        const float d = std::abs(s) <= FltEps ? 0 : std::max(a, b) * std::sqrt(1 / (s * s) - 1);

        orientation.Dir = -Vector3f::UnitZ();
        orientation.Up  = Vector3f::UnitY();
        orientation.Eye = Vector3f::UnitX() * sceneBBox.center().x()
                          + Vector3f::UnitY() * sceneBBox.center().y()
                          + Vector3f::UnitZ() * (sceneBBox.max.z() + d);
    }

    return orientation;
}
} // namespace IG