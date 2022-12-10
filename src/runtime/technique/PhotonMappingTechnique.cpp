#include "PhotonMappingTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
PhotonMappingTechnique::PhotonMappingTechnique(const Parser::Object& obj)
    : Technique("ppm")
{
    mPhotonCount    = (size_t)std::max(100, obj.property("photons").getInteger(1000000));
    mMaxCameraDepth = (size_t)obj.property("max_depth").isValid() ? obj.property("max_depth").getInteger(DefaultMaxRayDepth) : obj.property("max_camera_depth").getInteger(DefaultMaxRayDepth);
    mMaxLightDepth  = (size_t)obj.property("max_light_depth").getInteger(8);
    mLightSelector  = obj.property("light_selector").getString();
    mMergeRadius    = obj.property("radius").getNumber(0.01f);
    mClamp          = obj.property("clamp").getNumber(0.0f);
    mAOV            = obj.property("aov").getBool(false);
}

static std::string ppm_light_camera_generator(LoaderContext& ctx, const std::string& light_selector)
{
    std::stringstream stream;

    stream << RayGenerationShader::begin(ctx) << std::endl
           << ShaderUtils::generateDatabase(ctx) << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generate(tree, false) << std::endl
           << ctx.Lights->generateLightSelector(light_selector, tree)
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  let emitter = make_ppm_light_emitter(light_selector, settings.iter);" << std::endl
           << RayGenerationShader::end();

    return stream.str();
}

static std::string ppm_before_iteration_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    const auto technique      = ctx.Options.Scene.technique();
    const size_t mphotoncount = std::max(100, technique ? technique->property("photons").getInteger(1000000) : 1000000);

    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << "  let scene_bbox = " << LoaderUtils::inlineSceneBBox(ctx) << ";" << std::endl
           << "  ppm_handle_before_iteration(device, iter, " << ctx.CurrentTechniqueVariant << ", " << mphotoncount << ", scene_bbox);" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

TechniqueInfo PhotonMappingTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    // We got two passes. (0 -> Light Tracer, 1 -> Path Tracer with merging)
    info.Variants.resize(2);
    info.Variants[0].UsesLights = false; // LT makes no use of other lights (but starts on one)
    info.Variants[1].UsesLights = true;  // Standard PT still use of lights in the miss shader

    info.Variants[0].PrimaryPayloadCount = 7;
    info.Variants[1].PrimaryPayloadCount = 7;

    info.Variants[1].EmitterPayloadInitializer = "make_simple_payload_initializer(init_ppm_raypayload)";

    // To start from a light source, we do have to override the standard camera generator for LT
    info.Variants[0].OverrideCameraGenerator = [&](LoaderContext& ctx) { return ppm_light_camera_generator(ctx, mLightSelector); };

    // Each pass makes use of pre-iteration setups
    info.Variants[0].CallbackGenerators[(int)CallbackType::BeforeIteration] = ppm_before_iteration_generator; // Reset light cache
    info.Variants[1].CallbackGenerators[(int)CallbackType::BeforeIteration] = ppm_before_iteration_generator; // Construct query structure

    // The LT works independent of the framebuffer and requires a different work size
    info.Variants[0].OverrideWidth  = mPhotonCount; // Photon count
    info.Variants[0].OverrideHeight = 1;
    info.Variants[0].OverrideSPI    = 1; // The light tracer variant is always just one spi (Could be improved in the future though)

    info.Variants[0].LockFramebuffer = true; // We do not change the framebuffer

    if (mAOV) {
        info.EnabledAOVs.emplace_back("Direct Weights");
        info.EnabledAOVs.emplace_back("Merging Weights");
    }

    return info;
}

void PhotonMappingTechnique::generateBody(const SerializationInput& input) const
{
    const bool is_light_pass = input.Context.CurrentTechniqueVariant == 0;

    if (is_light_pass) {
        input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                     << "    match(id) {" << std::endl
                     << "      _ => make_empty_aov_image()" << std::endl
                     << "    }" << std::endl
                     << "  };" << std::endl;
    } else {
        if (mAOV) {
            input.Stream << "  let aov_di   = device.load_aov_image(\"Direct Weights\", spi); aov_di.mark_as_used();" << std::endl;
            input.Stream << "  let aov_merg = device.load_aov_image(\"Merging Weights\", spi); aov_merg.mark_as_used();" << std::endl;
        }

        input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                     << "    match(id) {" << std::endl;

        if (mAOV) {
            input.Stream << "      1 => aov_di," << std::endl
                         << "      2 => aov_merg," << std::endl;
        }

        input.Stream << "      _ => make_empty_aov_image()" << std::endl
                     << "    }" << std::endl
                     << "  };" << std::endl;
    }

    input.Stream << "  let scene_bbox  = " << LoaderUtils::inlineSceneBBox(input.Context) << ";" << std::endl
                 << "  let light_cache = make_ppm_lightcache(device, " << mPhotonCount << ", scene_bbox);" << std::endl;

    if (is_light_pass) {
        input.Stream << "  let technique = make_ppm_light_renderer(" << mMaxLightDepth << ", aovs, light_cache);" << std::endl;
    } else {
        ShadingTree tree(input.Context);
        input.Stream << input.Context.Lights->generateLightSelector("", tree)
                     << "  let ppm_radius = ppm_compute_radius(" << mMergeRadius * input.Context.SceneDiameter << ", settings.iter);" << std::endl
                     << "  let technique = make_ppm_path_renderer(" << mMaxCameraDepth << ",light_selector, ppm_radius, aovs, " << mClamp << ", light_cache);" << std::endl;
    }
}

} // namespace IG