#include "PathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
PathTechnique::PathTechnique(const SceneObject& obj)
    : Technique("path")
{
    mMaxDepth      = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mLightSelector = obj.property("light_selector").getString();
    mClamp         = obj.property("clamp").getNumber(0.0f);
    mMISAOVs       = obj.property("aov_mis").getBool(false);
}

TechniqueInfo PathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    if (mMISAOVs) {
        info.EnabledAOVs.emplace_back("Direct Weights");
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
    input.Context.GlobalRegistry.IntParameters["__tech_max_depth"] = (int)mMaxDepth;
    input.Context.GlobalRegistry.FloatParameters["__tech_clamp"]   = mClamp;

    if (mMaxDepth < 2) // 0 & 1 can be an optimization
        input.Stream << "  let tech_max_depth = " << mMaxDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_depth = registry::get_global_parameter_i32(\"__tech_max_depth\", 8);" << std::endl;

    if (mClamp <= 0) // 0 is a special case
        input.Stream << "  let tech_clamp = " << mClamp << ":f32;" << std::endl;
    else
        input.Stream << "  let tech_clamp = registry::get_global_parameter_f32(\"__tech_clamp\", 0);" << std::endl;

    // Handle AOVs
    if (mMISAOVs) {
        input.Stream << "  let aov_di  = device.load_aov_image(\"Direct Weights\", spi); aov_di.mark_as_used();" << std::endl
                     << "  let aov_nee = device.load_aov_image(\"NEE Weights\", spi); aov_nee.mark_as_used();" << std::endl;
    }

    input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                 << "    match(id) {" << std::endl;

    if (mMISAOVs) {
        input.Stream << "      1 => aov_di," << std::endl
                     << "      2 => aov_nee," << std::endl;
    }

    input.Stream << "      _ => make_empty_aov_image()" << std::endl
                 << "    }" << std::endl
                 << "  };" << std::endl;

    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree)
                 << "  let technique = make_path_renderer(tech_max_depth, light_selector, aovs, tech_clamp);" << std::endl;
}

} // namespace IG