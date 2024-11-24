#include "PhotonMappingTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
PhotonMappingTechnique::PhotonMappingTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("ppm")
    , mTechnique(obj)
{
    mPhotonCount   = (size_t)std::max(100, obj->property("photons").getInteger(1000000));
    mLightSelector = obj->property("light_selector").getString();
    mAOV           = obj->property("aov").getBool(false);
}

static std::string ppm_light_camera_generator(LoaderContext& ctx, const std::string& light_selector)
{
    std::stringstream stream;

    stream << RayGenerationShader::begin(ctx) << std::endl
           << ShaderUtils::generateDatabase(ctx) << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generate(tree, false) << std::endl
           << ctx.Lights->generateLightSelector(light_selector, tree)
           << "  let emitter = make_ppm_light_emitter(light_selector, render_config);" << std::endl
           << RayGenerationShader::end();

    return stream.str();
}

static std::string ppm_before_iteration_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << "  let tech_photons = registry::get_global_parameter_i32(\"__tech_photon_count\", 1000);" << std::endl
           << "  ppm_handle_before_iteration(device, settings.iter, " << ctx.CurrentTechniqueVariant << ", tech_photons, scene_bbox);" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

TechniqueInfo PhotonMappingTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    // We got two passes. (0 -> Light emission, 1 -> Path tracing with merging)
    info.Variants.resize(2);
    info.Variants[0].UsesLights = false; // LE makes no use of other lights (but starts on one)
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
    const bool is_light_pass = input.Tree.context().CurrentTechniqueVariant == 0;

    const std::string max_depth = mTechnique->hasProperty("max_depth") ? "max_depth" : "max_camera_depth";
    const std::string min_depth = mTechnique->hasProperty("min_depth") ? "min_depth" : "min_camera_depth";

    input.Tree.addInteger(max_depth, *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger(min_depth, *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("max_light_depth", *mTechnique, 8, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("radius", *mTechnique, 0.01f * input.Tree.context().SceneDiameter, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addComputedInteger("photon_count", (int)mPhotonCount, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());

    // Handle AOVs
    if (is_light_pass) {
        input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                     << "    match(id) {" << std::endl
                     << "      _ => make_empty_aov_image(0, 0)" << std::endl
                     << "    }" << std::endl
                     << "  };" << std::endl;
    } else {
        if (mAOV) {
            input.Stream << "  let aov_di   = device.load_aov_image(\"Direct Weights\", spi);" << std::endl;
            input.Stream << "  let aov_merg = device.load_aov_image(\"Merging Weights\", spi);" << std::endl;
        }

        input.Stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
                     << "    match(id) {" << std::endl;

        if (mAOV) {
            input.Stream << "      1 => aov_di," << std::endl
                         << "      2 => aov_merg," << std::endl;
        }

        input.Stream << "      _ => make_empty_aov_image(0, 0)" << std::endl
                     << "    }" << std::endl
                     << "  };" << std::endl;
    }

    input.Stream << "  let light_cache = make_ppm_lightcache(device, tech_photons, scene_bbox);" << std::endl;

    if (is_light_pass) {
        input.Stream << input.Tree.pullHeader()
                     << "  let technique = make_ppm_light_renderer(" << input.Tree.getInline("max_light_depth") << ", aovs, light_cache);" << std::endl;
    } else {
        input.Stream << input.Tree.pullHeader()
                     << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                     << "  let ppm_radius = ppm_compute_radius(" << input.Tree.getInline("radius") << ", settings.iter);" << std::endl
                     << "  let technique = make_ppm_path_renderer("
                     << input.Tree.getInline(max_depth)
                     << ", " << input.Tree.getInline(min_depth)
                     << ", light_selector, ppm_radius, aovs"
                     << ", " << input.Tree.getInline("clamp")
                     << ", light_cache);" << std::endl;
    }
}

} // namespace IG