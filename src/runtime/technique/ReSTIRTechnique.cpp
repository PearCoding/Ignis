#include "ReSTIRTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/ShaderUtils.h"

namespace IG {
ReSTIRTechnique::ReSTIRTechnique(const SceneObject& obj)
    : Technique("restir")
{
    mMaxDepth      = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mLightSelector = obj.property("light_selector").getString();
    mClamp         = obj.property("clamp").getNumber(0.0f);
    mMISAOVs       = obj.property("aov_mis").getBool(false);
}

static std::string restir_resampling_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << ShaderUtils::generateDatabase(ctx) << std::endl
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  resampling_pass(device, iter, spi, settings.frame);"
           << ShaderUtils::endCallback();

    return stream.str();
}

TechniqueInfo ReSTIRTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    info.EnabledAOVs.emplace_back("ReSTIR");
    info.EnabledAOVs.emplace_back("Direct Light");
    info.EnabledAOVs.emplace_back("Depth Info");

    info.Variants[0].ShadowHandlingMode        = ShadowHandlingMode::Advanced;
    info.Variants[0].RequiresExplicitCamera    = true;
    info.Variants[0].IsInteractive             = true;
    info.Variants[0].UsesLights                = true;
    info.Variants[0].PrimaryPayloadCount       = 6;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_rs_raypayload)";

    info.Variants[0].CallbackGenerators[(int)CallbackType::AfterIteration] = restir_resampling_generator;
    return info;
}

void ReSTIRTechnique::generateBody(const SerializationInput& input) const
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
    input.Stream << "  let aov_restir = device.load_aov_image(\"ReSTIR\", spi); aov_restir.mark_as_used();" << std::endl
                 << "  let aov_direct_light = device.load_aov_image(\"Direct Light\", spi); aov_direct_light.mark_as_used();" << std::endl
                 << "  let aov_depth_info = device.load_aov_image(\"Depth Info\", 1); aov_depth_info.mark_as_used();" << std::endl
                 << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                 << "    match(id) {" << std::endl
                 << "      1 => aov_restir," << std::endl
                 << "      2 => aov_direct_light," << std::endl
                 << "      3 => aov_depth_info," << std::endl
                 << "      _ => make_empty_aov_image()" << std::endl
                 << "    }" << std::endl
                 << "  };" << std::endl;

    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree)
                 << "  let technique = make_restir_renderer(camera, device, tech_max_depth, light_selector, aovs, tech_clamp, settings.frame);" << std::endl;
}

} // namespace IG