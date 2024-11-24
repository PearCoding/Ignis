#include "LightTracerTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
LightTracerTechnique::LightTracerTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("lighttracer")
    , mTechnique(obj)
{
    mLightSelector = obj->property("light_selector").getString();
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
    const std::string max_depth = mTechnique->hasProperty("max_depth") ? "max_depth" : "max_light_depth";
    const std::string min_depth = mTechnique->hasProperty("min_depth") ? "min_depth" : "min_light_depth";

    input.Tree.addInteger(max_depth, *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger(min_depth, *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << "  let framebuffer = device.load_aov_image(\"\", spi);" << std::endl
                 << "  let technique = make_lt_renderer(camera, framebuffer"
                 << ", " << input.Tree.getInline(max_depth)
                 << ", " << input.Tree.getInline(min_depth)
                 << ", " << input.Tree.getInline("clamp")
                 << ");" << std::endl;
}

} // namespace IG