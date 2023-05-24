#include "LightTracerTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
LightTracerTechnique::LightTracerTechnique(SceneObject& obj)
    : Technique("lighttracer")
{
    mMaxLightDepth = (size_t)obj.property("max_depth").isValid() ? obj.property("max_depth").getInteger(DefaultMaxRayDepth) : obj.property("max_light_depth").getInteger(DefaultMaxRayDepth);
    mMinLightDepth = (size_t)obj.property("min_depth").isValid() ? obj.property("min_depth").getInteger(DefaultMinRayDepth) : obj.property("min_light_depth").getInteger(DefaultMinRayDepth);

    mLightSelector = obj.property("light_selector").getString();
    mClamp         = obj.property("clamp").getNumber(0.0f);
}

static std::string lt_light_camera_generator(LoaderContext& ctx, const std::string& light_selector)
{
    std::stringstream stream;

    stream << RayGenerationShader::begin(ctx) << std::endl
           << ShaderUtils::generateDatabase(ctx) << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generate(tree, false) << std::endl
           << ctx.Lights->generateLightSelector(light_selector, tree)
           << "  let emitter = make_lt_emitter(light_selector, render_config);" << std::endl
           << RayGenerationShader::end();

    return stream.str();
}

TechniqueInfo LightTracerTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    info.Variants[0].UsesLights = false; // LT makes no use of other lights (but starts on one)

    info.Variants[0].PrimaryPayloadCount   = 5;
    info.Variants[0].SecondaryPayloadCount = 2;

    // To start from a light source, we do have to override the standard camera generator for LT
    info.Variants[0].OverrideCameraGenerator = [&](LoaderContext& ctx) { return lt_light_camera_generator(ctx, mLightSelector); };

    info.Variants[0].RequiresExplicitCamera = true;
    info.Variants[0].ShadowHandlingMode     = ShadowHandlingMode::Advanced;

    return info;
}

void LightTracerTechnique::generateBody(const SerializationInput& input) const
{
    // Insert config into global registry
    input.Context.GlobalRegistry.IntParameters["__tech_max_depth"] = (int)mMaxLightDepth;
    input.Context.GlobalRegistry.IntParameters["__tech_min_depth"] = (int)mMinLightDepth;
    input.Context.GlobalRegistry.FloatParameters["__tech_clamp"]   = mClamp;

    if (mMaxLightDepth < 2) // 0 & 1 can be an optimization
        input.Stream << "  let tech_max_depth = " << mMaxLightDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_depth = registry::get_global_parameter_i32(\"__tech_max_depth\", 8);" << std::endl;

    if (mMinLightDepth < 2) // 0 & 1 can be an optimization
        input.Stream << "  let tech_min_depth = " << mMinLightDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_min_depth = registry::get_global_parameter_i32(\"__tech_min_depth\", 2);" << std::endl;

    if (mClamp <= 0) // 0 is a special case
        input.Stream << "  let tech_clamp = " << mClamp << ":f32;" << std::endl;
    else
        input.Stream << "  let tech_clamp = registry::get_global_parameter_f32(\"__tech_clamp\", 0);" << std::endl;

    input.Stream << "  let framebuffer = device.load_aov_image(\"\", spi);" << std::endl
                 << "  let technique = make_lt_renderer(camera, framebuffer, tech_max_depth, tech_min_depth, tech_clamp);" << std::endl;
}

} // namespace IG