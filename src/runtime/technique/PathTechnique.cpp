#include "PathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
PathTechnique::PathTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("path")
    , mTechnique(obj)
{
    mLightSelector = obj->property("light_selector").getString();
    mEnableNEE     = obj->property("nee").getBool(true);
    mMISAOVs       = obj->property("aov_mis").getBool(false);
}

TechniqueInfo PathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mMISAOVs)
        info.Passes[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;

    info.UsesLights                = true;
    info.PrimaryPayloadCount       = 6;
    info.EmitterPayloadInitializer = "make_simple_payload_initializer(init_pt_raypayload)";
    return info;
}

void PathTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("min_depth", *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = make_path_renderer("
                 << "device"
                 << ", " << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", spi"
                 << ", light_selector"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false")
                 << ", " << (mMISAOVs ? "true" : "false") << ");" << std::endl;
}

} // namespace IG