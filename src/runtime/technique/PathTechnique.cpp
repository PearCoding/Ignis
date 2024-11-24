#include "PathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
PathTechnique::PathTechnique(SceneObject& obj)
    : Technique("path")
{
    mMaxDepth      = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mMinDepth      = obj.property("min_depth").getInteger(DefaultMinRayDepth);
    mLightSelector = obj.property("light_selector").getString();
    mClamp         = obj.property("clamp").getNumber(0.0f);
    mEnableNEE     = obj.property("nee").getBool(true);
    mMISAOVs       = obj.property("aov_mis").getBool(false);
}

TechniqueInfo PathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mMISAOVs) {
        info.EnabledAOVs.emplace_back("BSDF Weights");
        info.EnabledAOVs.emplace_back("NEE Weights");
        info.Variants[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;
    }

    info.Variants[0].UsesLights                = true;
    info.Variants[0].PrimaryPayloadCount       = 6;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_pt_raypayload)";
    return info;
}

void PathTechnique::generateBody(const SerializationInput& input) const
{
    // Insert config into global registry
    input.Tree.context().GlobalRegistry.IntParameters["__tech_max_depth"] = (int)mMaxDepth;
    input.Tree.context().GlobalRegistry.IntParameters["__tech_min_depth"] = (int)mMinDepth;
    input.Tree.context().GlobalRegistry.FloatParameters["__tech_clamp"]   = mClamp;

    if (mMaxDepth < 2 && input.Tree.context().Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 & 1 can be an optimization // TODO: Unlikely an optimization. Maybe get rid of it
        input.Stream << "  let tech_max_depth = " << mMaxDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_depth = registry::get_global_parameter_i32(\"__tech_max_depth\", 8);" << std::endl;

    if (mMinDepth < 2 && input.Tree.context().Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 & 1 can be an optimization // TODO: Unlikely an optimization. Maybe get rid of it
        input.Stream << "  let tech_min_depth = " << mMinDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_min_depth = registry::get_global_parameter_i32(\"__tech_min_depth\", 2);" << std::endl;

    if (mClamp <= 0 && input.Tree.context().Options.Specialization != RuntimeOptions::SpecializationMode::Disable) // 0 is a special case
        input.Stream << "  let tech_clamp = " << mClamp << ":f32;" << std::endl;
    else
        input.Stream << "  let tech_clamp = registry::get_global_parameter_f32(\"__tech_clamp\", 0);" << std::endl;

    // Handle AOVs
    if (mMISAOVs) {
        input.Stream << "  let aov_di  = device.load_aov_image(\"BSDF Weights\", spi);" << std::endl
                     << "  let aov_nee = device.load_aov_image(\"NEE Weights\", spi);" << std::endl;
    }

    input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                 << "    match(id) {" << std::endl;

    if (mMISAOVs) {
        input.Stream << "      1 => aov_di," << std::endl
                     << "      2 => aov_nee," << std::endl;
    }

    input.Stream << "      _ => make_empty_aov_image(0, 0)" << std::endl
                 << "    }" << std::endl
                 << "  };" << std::endl;

    ShadingTree tree(input.Tree.context());
    input.Stream << input.Tree.context().Lights->generateLightSelector(mLightSelector, tree)
                 << "  let technique = make_path_renderer(tech_max_depth, tech_min_depth, light_selector, aovs, tech_clamp,"
                 << (mEnableNEE ? "true" : "false") << ");" << std::endl;
}

} // namespace IG