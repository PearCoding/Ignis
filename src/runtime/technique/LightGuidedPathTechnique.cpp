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

    if (mMISAOVs) {
        info.EnabledAOVs.emplace_back("BSDF Weights");
        info.EnabledAOVs.emplace_back("Guided Weights");
        info.EnabledAOVs.emplace_back("NEE Weights");
        info.Variants[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;
    }

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

    // Handle AOVs
    if (mMISAOVs) {
        input.Stream << "  let aov_direct = device.load_aov_image(\"BSDF Weights\", spi);" << std::endl
                     << "  let aov_guided = device.load_aov_image(\"Guided Weights\", spi);" << std::endl
                     << "  let aov_nee    = device.load_aov_image(\"NEE Weights\", spi);" << std::endl;
    }

    input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                 << "    match(id) {" << std::endl;

    if (mMISAOVs) {
        input.Stream << "      1 => aov_direct," << std::endl
                     << "      2 => aov_guided," << std::endl
                     << "      3 => aov_nee," << std::endl;
    }

    input.Stream << "      _ => make_empty_aov_image(0, 0)" << std::endl
                 << "    }" << std::endl
                 << "  };" << std::endl;

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = make_light_sgpt_renderer("
                 << input.Tree.getInline("max_depth")
                 << ", " << input.Tree.getInline("min_depth")
                 << ", light_selector, aovs"
                 << ", " << input.Tree.getInline("clamp")
                 << ", " << (mEnableNEE ? "true" : "false") << ", infinite_lights.get(0) /*TODO*/"
                 << ", " << input.Tree.getInline("defensive")
                 << ");" << std::endl;
}

} // namespace IG