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

static std::string ppm_camera_generator(LoaderContext& ctx, const std::string& light_selector)
{
    ShadingTree tree(ctx);

    std::stringstream stream;
    stream << RayGenerationShader::begin(ctx) << std::endl
           << ShaderUtils::generateDatabase(ctx) << std::endl
           << ctx.Lights->generate(tree, false) << std::endl
           << ctx.Lights->generateLightSelector(light_selector, tree)
           << "  let emitter_lt = make_ppm_light_emitter(light_selector, render_config);" << std::endl
           << RayGenerationShader::generateDefaultCamera(ctx, "emitter_pt") << std::endl
           << "  let emitter = if settings.pass == 0 { emitter_lt } else { emitter_pt };" << std::endl
           << RayGenerationShader::end();

    return stream.str();
}

static std::string ppm_before_iteration_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << "  let tech_photons = registry::get_global_parameter_i32(\"__tech_photon_count\", 1000);" << std::endl
           << "  ppm_handle_before_iteration(device, settings.iter, settings.pass, tech_photons, scene_bbox);" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

TechniqueInfo PhotonMappingTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;

    info.UsesLights          = true; // Standard PT still uses lights in the miss shader
    info.PrimaryPayloadCount = 7;

    info.EmitterPayloadInitializer = "make_simple_payload_initializer(init_ppm_raypayload)";

    // To start from a light source, we do have to override the standard camera generator for LT
    info.OverrideCameraGenerator = [&](LoaderContext& ctx) { return ppm_camera_generator(ctx, mLightSelector); };

    // Each pass makes use of pre-iteration setups
    info.CallbackGenerators[(int)CallbackType::BeforePass] = ppm_before_iteration_generator;

    // We got two passes. (0 -> Light emission, 1 -> Path tracing with merging)
    info.Passes.resize(2);
    // The LT works independent of the framebuffer and requires a different work size
    info.Passes[0].OverrideWidth   = mPhotonCount; // Photon count
    info.Passes[0].OverrideHeight  = 1;
    info.Passes[0].OverrideSPI     = 1;    // The light tracer variant is always just one spi (Could be improved in the future though)
    info.Passes[0].LockFramebuffer = true; // We do not change the framebuffer

    return info;
}

void PhotonMappingTechnique::generateBody(const SerializationInput& input) const
{
    const std::string max_depth = mTechnique->hasProperty("max_depth") ? "max_depth" : "max_camera_depth";
    const std::string min_depth = mTechnique->hasProperty("min_depth") ? "min_depth" : "min_camera_depth";

    input.Tree.addComputedInteger("photon_count", (int)mPhotonCount, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger("max_light_depth", *mTechnique, 8, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger(max_depth, *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addInteger(min_depth, *mTechnique, DefaultMinRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("radius", *mTechnique, 0.01f * input.Tree.context().SceneDiameter, ShadingTree::NumberOptions::Zero().MakeGlobal());
    input.Tree.addNumber("clamp", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << "  let light_cache = make_ppm_lightcache(device, " << input.Tree.getInline("photon_count") << ", scene_bbox);" << std::endl
                 << "  let technique_lt = make_ppm_light_renderer(" << input.Tree.getInline("max_light_depth") << ", light_cache);" << std::endl
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let ppm_radius = ppm_compute_radius(" << input.Tree.getInline("radius") << ", settings.iter);" << std::endl
                 << "  let technique_pt = make_ppm_path_renderer(device"
                 << ", " << input.Tree.getInline(max_depth)
                 << ", " << input.Tree.getInline(min_depth)
                 << ", spi, light_selector, ppm_radius"
                 << ", " << input.Tree.getInline("clamp")
                 << ", light_cache"
                 << ", " << (mAOV ? "true" : "false") << ");" << std::endl
                 << "  let technique = if settings.pass == 0 { technique_lt } else { technique_pt };" << std::endl;
}

} // namespace IG