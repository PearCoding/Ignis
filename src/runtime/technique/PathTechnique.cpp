#include "PathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
PathTechnique::PathTechnique(const Parser::Object& obj)
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
                 << "  let technique = make_path_renderer(" << mMaxDepth << ", light_selector, aovs, " << mClamp << ");" << std::endl;
}

} // namespace IG