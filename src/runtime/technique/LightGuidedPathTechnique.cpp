#include "LightGuidedPathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
LightGuidedPathTechnique::LightGuidedPathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("lsgpt")
    , mTechnique(obj)
{
    mLightSelector = obj->property("light_selector").getString();
    mEnableNEE     = obj->property("nee").getBool(true);
    mMISAOVs       = obj->property("aov_mis").getBool(false);
}

TechniqueInfo LightGuidedPathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mMISAOVs)
        info.Variants[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;

    info.Variants[0].UsesLights                = true;
    info.Variants[0].PrimaryPayloadCount       = 8;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_sgpt_raypayload)";
    return info;
}

void LightGuidedPathTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addNumber("defensive", *mTechnique, 0.3f, ShadingTree::NumberOptions::Dynamic().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = make_light_sgpt_renderer(device"
                 << ", " << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", spi"
                 << ", light_selector"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false") 
                 << ", " << (mMISAOVs ? "true" : "false") 
                 << ", infinite_lights.get(0) /*TODO*/"
                 << ", " << input.Tree.getInline("defensive")
                 << ");" << std::endl;
}

} // namespace IG