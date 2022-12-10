#include "LightTracerTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
LightTracerTechnique::LightTracerTechnique(const Parser::Object& obj)
    : Technique("lighttracer")
{
    mMaxLightDepth = (size_t)obj.property("max_depth").isValid() ? obj.property("max_depth").getInteger(DefaultMaxRayDepth) : obj.property("max_light_depth").getInteger(DefaultMaxRayDepth);
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
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  let emitter = make_lt_light_emitter(light_selector, settings.iter);" << std::endl
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
    input.Stream << "  let framebuffer = device.load_aov_image(\"\", spi);" << std::endl
                 << "  let technique = make_lt_renderer(camera, framebuffer, " << mMaxLightDepth << ", " << mClamp << ");" << std::endl;
}

} // namespace IG