#include "LoaderCamera.h"
#include "Loader.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "StringUtils.h"
#include "camera/FishLensCamera.h"
#include "camera/NullCamera.h"
#include "camera/OrthogonalCamera.h"
#include "camera/PerspectiveCamera.h"

namespace IG {

static std::shared_ptr<Camera> camera_perspective(const std::shared_ptr<Parser::Object>& camera)
{
    return std::make_shared<PerspectiveCamera>(*camera);
}
static std::shared_ptr<Camera> camera_orthogonal(const std::shared_ptr<Parser::Object>& camera)
{
    return std::make_shared<OrthogonalCamera>(*camera);
}

static std::shared_ptr<Camera> camera_fishlens(const std::shared_ptr<Parser::Object>& camera)
{
    return std::make_shared<FishLensCamera>(*camera);
}

using CameraConstructor = std::shared_ptr<Camera> (*)(const std::shared_ptr<Parser::Object>&);
static const struct CameraEntry {
    const char* Name;
    CameraConstructor Constructor;
} _generators[] = {
    { "perspective", camera_perspective },
    { "orthogonal", camera_orthogonal },
    { "fishlens", camera_fishlens },
    { "fisheye", camera_fishlens },
    { "", nullptr }
};

static const CameraEntry* getCameraEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _generators[i].Constructor; ++i) {
        if (_generators[i].Name == lower_name)
            return &_generators[i];
    }
    IG_LOG(L_ERROR) << "No camera type '" << name << "' available" << std::endl;
    return nullptr;
}

void LoaderCamera::setup(const LoaderContext& ctx)
{
    if (ctx.Options.IsTracer) {
        mCamera = std::make_shared<NullCamera>();
        return;
    }

    const auto entry = getCameraEntry(ctx.Options.CameraType);
    if (!entry)
        return;

    auto camera = ctx.Options.Scene.camera();
    if (!camera)
        camera = std::make_shared<Parser::Object>(Parser::OT_CAMERA, "", ctx.Options.FilePath.parent_path()); // Create a default variant

    mCamera = entry->Constructor(camera);

    IG_LOG(L_DEBUG) << "Using camera: '" << mCamera->type() << "'" << std::endl;
}

std::string LoaderCamera::generate(LoaderContext& ctx) const
{
    if (!mCamera)
        return {};

    std::stringstream stream;
    mCamera->serialize(Camera::SerializationInput{ stream, ctx });

    stream << "  maybe_unused(camera);" << std::endl;

    return stream.str();
}

CameraOrientation LoaderCamera::getOrientation(const LoaderContext& ctx) const
{
    if (!mCamera)
        return CameraOrientation();

    return mCamera->getOrientation(ctx);
}

std::vector<std::string> LoaderCamera::getAvailableTypes()
{
    std::vector<std::string> array;

    for (size_t i = 0; _generators[i].Constructor; ++i)
        array.emplace_back(_generators[i].Name);

    std::sort(array.begin(), array.end());

    return array;
}
} // namespace IG