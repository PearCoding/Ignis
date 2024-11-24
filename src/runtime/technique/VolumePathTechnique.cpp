#include "VolumePathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
VolumePathTechnique::VolumePathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("volpath")
    , mTechnique(obj)
{
    mLightSelector = obj->property("light_selector").getString();
    mEnableNEE     = obj->property("nee").getBool(true);
}

TechniqueInfo VolumePathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights                = true;
    info.Variants[0].UsesMedia                 = true;
    info.Variants[0].PrimaryPayloadCount       = 7;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_vpt_raypayload)";

    return info;
}

void VolumePathTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let aovs = @|_id:i32| make_empty_aov_image(0, 0);" << std::endl
                 << "  let technique = make_volume_path_renderer("
                 << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", light_selector, media, aovs"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false") << ");" << std::endl;
}

} // namespace IG