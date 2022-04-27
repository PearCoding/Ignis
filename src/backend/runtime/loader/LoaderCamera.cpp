#include "LoaderCamera.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <chrono>

namespace IG {

// TODO: Sometimes we have to "fix" the registry and width & height of the image buffer. Make this an option

static CameraOrientation camera_perspective_orientation(const std::string&, const std::shared_ptr<Parser::Object>& camera, const LoaderContext& ctx)
{
    CameraOrientation orientation;
    if (camera && camera->property("transform").isValid()) {
        const auto cameraTransform = camera->property("transform").getTransform();
        orientation.Eye            = cameraTransform * Vector3f::Zero();
        orientation.Dir            = cameraTransform.linear().col(2);
        orientation.Up             = cameraTransform.linear().col(1);
    } else {
        float fov            = camera ? camera->property("fov").getNumber(60) : 60;
        const auto sceneBBox = ctx.Environment.SceneBBox;
        // Try to setup a view over the whole scene
        const float aspect_ratio = ctx.FilmWidth / static_cast<float>(ctx.FilmHeight);
        const float a            = sceneBBox.diameter().x() / 2;
        const float b            = sceneBBox.diameter().y() / (2 * aspect_ratio);
        const float s            = std::sin(fov * Deg2Rad / 2);
        const float d            = std::max(a, b) * std::sqrt(1 / (s * s) - 1);

        orientation.Dir = -Vector3f::UnitZ();
        orientation.Up  = Vector3f::UnitY();
        orientation.Eye = Vector3f::UnitX() * sceneBBox.center().x()
                          + Vector3f::UnitY() * sceneBBox.center().y()
                          + Vector3f::UnitZ() * (sceneBBox.max.z() + d);
    }

    return orientation;
}

static void camera_perspective(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& camera, const LoaderContext& ctx)
{
    float fov  = camera ? camera->property("fov").getNumber(60) : 60;
    float tmin = camera ? camera->property("near_clip").getNumber(0) : 0;
    float tmax = camera ? camera->property("far_clip").getNumber(std::numeric_limits<float>::max()) : std::numeric_limits<float>::max();

    float focal_length    = camera ? camera->property("focal_length").getNumber(1) : 1;
    float aperture_radius = camera ? camera->property("aperture_radius").getNumber(0) : 0;

    if (tmax < tmin)
        std::swap(tmin, tmax);

    CameraOrientation orientation = camera_perspective_orientation(name, camera, ctx);

    // Dump camera control (above is just defaults)
    stream << "  let camera_eye = registry::get_parameter_vec3(\"__camera_eye\", " << ShaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
           << "  let camera_dir = registry::get_parameter_vec3(\"__camera_dir\", " << ShaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
           << "  let camera_up  = registry::get_parameter_vec3(\"__camera_up\" , " << ShaderUtils::inlineVector(orientation.Up) << ");" << std::endl;

    if (std::abs(focal_length - 1) <= FltEps && aperture_radius <= FltEps) {
        stream << "  let camera = make_perspective_camera(camera_eye, " << std::endl
               << "    camera_dir, " << std::endl
               << "    camera_up, " << std::endl
               << "    settings.width as f32, " << std::endl
               << "    settings.height as f32, " << std::endl
               << "    " << fov * Deg2Rad << ", " << std::endl
               << "    " << tmin << ", " << std::endl
               << "    " << tmax << ");" << std::endl;
    } else {
        stream << "  let camera = make_perspective_dof_camera(camera_eye, " << std::endl
               << "    camera_dir, " << std::endl
               << "    camera_up, " << std::endl
               << "    settings.width as f32, " << std::endl
               << "    settings.height as f32, " << std::endl
               << "    " << fov * Deg2Rad << ", " << std::endl
               << "    " << aperture_radius << ", " << std::endl
               << "    " << focal_length << ", " << std::endl
               << "    " << tmin << ", " << std::endl
               << "    " << tmax << ");" << std::endl;
    }
}

static CameraOrientation camera_orthogonal_orientation(const std::string&, const std::shared_ptr<Parser::Object>& camera, const LoaderContext&)
{
    const auto cameraTransform = camera ? camera->property("transform").getTransform() : Transformf::Identity();

    CameraOrientation orientation;
    orientation.Eye = cameraTransform * Vector3f::Zero();
    orientation.Dir = cameraTransform.linear().col(2);
    orientation.Up  = cameraTransform.linear().col(1);
    // TODO: Maybe add a scene dependent orientation?
    return orientation;
}

static void camera_orthogonal(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& camera, const LoaderContext& ctx)
{
    float tmin = camera ? camera->property("near_clip").getNumber(0) : 0;
    float tmax = camera ? camera->property("far_clip").getNumber(std::numeric_limits<float>::max()) : std::numeric_limits<float>::max();

    if (tmax < tmin)
        std::swap(tmin, tmax);

    CameraOrientation orientation = camera_orthogonal_orientation(name, camera, ctx);

    // Dump camera control (above is just defaults)
    stream << "  let camera_eye = registry::get_parameter_vec3(\"__camera_eye\", " << ShaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
           << "  let camera_dir = registry::get_parameter_vec3(\"__camera_dir\", " << ShaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
           << "  let camera_up  = registry::get_parameter_vec3(\"__camera_up\" , " << ShaderUtils::inlineVector(orientation.Up) << ");" << std::endl
           << "  let camera = make_orthogonal_camera(camera_eye, " << std::endl
           << "    camera_dir, " << std::endl
           << "    camera_up, " << std::endl
           << "    settings.width as f32, " << std::endl
           << "    settings.height as f32, " << std::endl
           << "    " << tmin << ", " << std::endl
           << "    " << tmax << ");" << std::endl;
}

static void camera_fishlens(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& camera, const LoaderContext& ctx)
{
    float tmin = camera ? camera->property("near_clip").getNumber(0) : 0;
    float tmax = camera ? camera->property("far_clip").getNumber(std::numeric_limits<float>::max()) : std::numeric_limits<float>::max();

    if (tmax < tmin)
        std::swap(tmin, tmax);

    CameraOrientation orientation = camera_orthogonal_orientation(name, camera, ctx);

    std::string mode_str = camera ? camera->property("mode").getString("") : "";

    std::string mode = "FisheyeAspectMode::Circular";
    if (mode == "cropped")
        mode = "FisheyeAspectMode::Cropped";
    else if (mode == "full")
        mode = "FisheyeAspectMode::Full";

    // Dump camera control (above is just defaults)
    stream << "  let camera_eye = registry::get_parameter_vec3(\"__camera_eye\", " << ShaderUtils::inlineVector(orientation.Eye) << ");" << std::endl
           << "  let camera_dir = registry::get_parameter_vec3(\"__camera_dir\", " << ShaderUtils::inlineVector(orientation.Dir) << ");" << std::endl
           << "  let camera_up  = registry::get_parameter_vec3(\"__camera_up\" , " << ShaderUtils::inlineVector(orientation.Up) << ");" << std::endl
           << "  let camera = make_fishlens_camera(camera_eye, " << std::endl
           << "    camera_dir, " << std::endl
           << "    camera_up, " << std::endl
           << "    settings.width as f32, " << std::endl
           << "    settings.height as f32, " << std::endl
           << "    " << mode << ", " << std::endl
           << "    " << tmin << ", " << std::endl
           << "    " << tmax << ");" << std::endl;
}

using CameraLoader           = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);
using CameraSetupOrientation = CameraOrientation (*)(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);
static const struct CameraEntry {
    const char* Name;
    CameraLoader Loader;
    CameraSetupOrientation SetupOrientation;
} _generators[] = {
    { "perspective", camera_perspective, camera_perspective_orientation },
    { "orthogonal", camera_orthogonal, camera_orthogonal_orientation },
    { "fishlens", camera_fishlens, camera_orthogonal_orientation /* For now */ },
    { "fisheye", camera_fishlens, camera_orthogonal_orientation /* For now */ },
    { "", nullptr, nullptr }
};

static const CameraEntry* getCameraEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _generators[i].Loader; ++i) {
        if (_generators[i].Name == lower_name)
            return &_generators[i];
    }
    IG_LOG(L_ERROR) << "No camera type '" << name << "' available" << std::endl;
    return nullptr;
}

std::string LoaderCamera::generate(const LoaderContext& ctx)
{
    if (ctx.IsTracer)
        return "  let camera = make_null_camera(); maybe_unused(camera);";

    const auto entry = getCameraEntry(ctx.CameraType);
    if (!entry)
        return {};

    const auto camera = ctx.Scene.camera();

    std::stringstream stream;
    entry->Loader(stream, ctx.CameraType, camera, ctx);

    stream << "  maybe_unused(camera);" << std::endl;

    return stream.str();
}

void LoaderCamera::setupInitialOrientation(const LoaderContext& ctx, LoaderResult& result)
{
    const auto entry = getCameraEntry(ctx.CameraType);
    if (!entry)
        return;

    const auto camera        = ctx.Scene.camera();
    result.CameraOrientation = entry->SetupOrientation(ctx.CameraType, camera, ctx);
}

} // namespace IG