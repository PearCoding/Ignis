#include "LoaderTechnique.h"
#include "Loader.h"
#include "LoaderCamera.h"
#include "LoaderLight.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "StringUtils.h"
#include "light/LightHierarchy.h"
#include "technique/AOTechnique.h"
#include "technique/CameraCheckTechnique.h"
#include "technique/DebugTechnique.h"
#include "technique/InfoBufferTechnique.h"
#include "technique/LightTracerTechnique.h"
#include "technique/LightVisibilityTechnique.h"
#include "technique/PathTechnique.h"
#include "technique/PhotonMappingTechnique.h"
#include "technique/Technique.h"
#include "technique/VolumePathTechnique.h"
#include "technique/WireframeTechnique.h"

#include <numeric>

namespace IG {

static std::shared_ptr<Technique> ao_loader(const std::shared_ptr<SceneObject>&)
{
    return std::make_shared<AOTechnique>();
}
static std::shared_ptr<Technique> cc_loader(const std::shared_ptr<SceneObject>&)
{
    return std::make_shared<CameraCheckTechnique>();
}
static std::shared_ptr<Technique> debug_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<DebugTechnique>(*obj);
}
static std::shared_ptr<Technique> ib_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<InfoBufferTechnique>(*obj);
}
static std::shared_ptr<Technique> lt_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<LightTracerTechnique>(*obj);
}
static std::shared_ptr<Technique> lv_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<LightVisibilityTechnique>(*obj);
}
static std::shared_ptr<Technique> pt_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PathTechnique>(*obj);
}
static std::shared_ptr<Technique> ppm_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<PhotonMappingTechnique>(*obj);
}
static std::shared_ptr<Technique> vpt_loader(const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<VolumePathTechnique>(*obj);
}
static std::shared_ptr<Technique> wf_loader(const std::shared_ptr<SceneObject>&)
{
    return std::make_shared<WireframeTechnique>();
}

// Will return information about the enabled AOVs
using TechniqueConstructor = std::shared_ptr<Technique> (*)(const std::shared_ptr<SceneObject>&);

static const struct TechniqueEntry {
    const char* Name;
    TechniqueConstructor Constructor;
} _generators[] = {
    { "ao", ao_loader },
    { "pt", pt_loader },
    { "path", pt_loader },
    { "volpath", vpt_loader },
    { "debug", debug_loader },
    { "ppm", ppm_loader },
    { "photonmapper", ppm_loader },
    { "lt", lt_loader },
    { "lighttracer", lt_loader },
    { "wireframe", wf_loader },
    { "infobuffer", ib_loader },
    { "lightvisibility", lv_loader },
    { "camera_check", cc_loader },
    { "", nullptr }
};

static const TechniqueEntry* getTechniqueEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _generators[i].Constructor; ++i) {
        if (_generators[i].Name == lower_name)
            return &_generators[i];
    }
    IG_LOG(L_ERROR) << "No technique type '" << name << "' available" << std::endl;
    return nullptr;
}

LoaderTechnique::LoaderTechnique()  = default;
LoaderTechnique::~LoaderTechnique() = default;

void LoaderTechnique::setup(const LoaderContext& ctx)
{
    const auto* entry = getTechniqueEntry(ctx.Options.TechniqueType);
    if (!entry)
        return;

    auto technique = ctx.Options.Scene->technique();
    if (!technique)
        technique = std::make_shared<SceneObject>(SceneObject::OT_TECHNIQUE, "", ctx.Options.FilePath.parent_path()); // Create a default variant

    mTechnique = entry->Constructor(technique);
    if (!mTechnique)
        return;

    IG_LOG(L_DEBUG) << "Using technique: '" << mTechnique->type() << "'" << std::endl;

    mInfo = mTechnique->getInfo(ctx);
    if (ctx.Options.Glare.Enabled || (ctx.Options.Denoiser.Enabled && mTechnique->hasDenoiserSupport()))
        InfoBufferTechnique::enable(mInfo, !ctx.Options.Denoiser.OnlyFirstIteration);
}

std::string LoaderTechnique::generate(LoaderContext& ctx)
{
    if (!mTechnique)
        return {};

    std::stringstream stream;

    // Handle denoiser if necessary
    if (mTechnique->hasDenoiserSupport() && InfoBufferTechnique::insertBody(Technique::SerializationInput{ stream, ctx }, 8 /* TODO */, false))
        return stream.str();

    mTechnique->generateBody(Technique::SerializationInput{ stream, ctx });
    return stream.str();
}

bool LoaderTechnique::hasDenoiserEnabled() const
{
    const auto& normal_it = std::find(mInfo.EnabledAOVs.begin(), mInfo.EnabledAOVs.end(), "Normals");
    return normal_it != mInfo.EnabledAOVs.end();
}

std::vector<std::string> LoaderTechnique::getAvailableTypes()
{
    std::vector<std::string> array;

    for (size_t i = 0; _generators[i].Constructor; ++i)
        array.emplace_back(_generators[i].Name);

    std::sort(array.begin(), array.end());
    return array;
}
} // namespace IG