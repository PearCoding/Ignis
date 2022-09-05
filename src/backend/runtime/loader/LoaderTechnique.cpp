#include "LoaderTechnique.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "LoaderUtils.h"
#include "Logger.h"
#include "ShadingTree.h"
#include "light/LightHierarchy.h"
#include "serialization/VectorSerializer.h"
#include "shader/RayGenerationShader.h"
#include "shader/ShaderUtils.h"

#include <numeric>

namespace IG {
// FIXME: The convergence rate is kinda bad and some have a slight bias, keep it uniform until fixes
static const std::string DefaultLightSelector = "uniform";

static void technique_empty_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    stream << "static RayPayloadComponents = 0;" << std::endl
           << "fn init_raypayload() = make_empty_payload();" << std::endl;
}

static TechniqueInfo technique_empty_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    return {};
}

/////////////////////////

static void ao_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, LoaderContext&)
{
    stream << "  let technique = make_ao_renderer();" << std::endl;
}

/////////////////////////

static void debug_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, LoaderContext&)
{
    // TODO: Maybe add a changeable default mode?
    stream << "  let debug_mode = registry::get_global_parameter_i32(\"__debug_mode\", 0);" << std::endl
           << "  let technique  = make_debug_renderer(debug_mode);" << std::endl;
}

static TechniqueInfo debug_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true; // We make use of the emissive information!
    return info;
}

/////////////////////////

static void wireframe_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, LoaderContext&)
{
    // Camera was defined by RequiresExplicitCamera flag
    stream << "  let technique = make_wireframe_renderer(camera);" << std::endl;
}

static void wireframe_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    constexpr int C = 1 /* Depth */ + 1 /* Distance */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = wrap_wireframeraypayload(WireframeRayPayload { depth = 1, distance = 0 });" << std::endl;
}

static TechniqueInfo wireframe_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    TechniqueInfo info;
    info.Variants[0].RequiresExplicitCamera = true; // We make use of the camera differential!
    return info;
}

/////////////////////////

static void enable_ib(TechniqueInfo& info, bool always = false, bool extend = true)
{
    info.EnabledAOVs.emplace_back("Normals");
    info.EnabledAOVs.emplace_back("Albedo");
    info.EnabledAOVs.emplace_back("Depth");

    if (extend)
        info.Variants.emplace_back();

    info.Variants.back().LockFramebuffer = true;
    info.Variants.back().OverrideSPI     = 1;

    const size_t variantCount = info.Variants.size();
    if (variantCount > 1) {
        if (info.VariantSelector) {
            const auto prevSelector = info.VariantSelector;
            info.VariantSelector    = [=](size_t iter) {
                if (always || iter == 0) {
                    std::vector<size_t> pass_with_ib = prevSelector(iter);
                    pass_with_ib.emplace_back(variantCount - 1);
                    return pass_with_ib;
                } else {
                    return prevSelector(iter);
                }
            };
        } else {
            info.VariantSelector = [=](size_t iter) {
                if (always || iter == 0) {
                    std::vector<size_t> pass_with_ib(variantCount);
                    std::iota(std::begin(pass_with_ib), std::end(pass_with_ib), 0);
                    return pass_with_ib;
                } else {
                    std::vector<size_t> pass_without_ib(variantCount - 1);
                    std::iota(std::begin(pass_without_ib), std::end(pass_without_ib), 0);
                    return pass_without_ib;
                }
            };
        }
    }
}

static bool handle_ib_body(std::ostream& stream, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    const auto& info = ctx.TechniqueInfo;

    const auto& normal_it = std::find(info.EnabledAOVs.begin(), info.EnabledAOVs.end(), "Normals");
    if (normal_it == info.EnabledAOVs.end())
        return false;

    if (ctx.CurrentTechniqueVariant != info.Variants.size() - 1)
        return false;

    const int max_depth        = technique ? technique->property("max_depth").getInteger(64) : 64;
    const bool handle_specular = ctx.Denoiser.FollowSpecular || (technique ? technique->property("denoiser_handle_specular").getBool(false) : false);

    stream << "  let aov_normals = device.load_aov_image(\"Normals\", spi); aov_normals.mark_as_used();" << std::endl;
    stream << "  let aov_albedo = device.load_aov_image(\"Albedo\", spi); aov_albedo.mark_as_used();" << std::endl;
    stream << "  let aov_depth  = device.load_aov_image(\"Depth\", spi); aov_depth.mark_as_used();" << std::endl;

    stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
           << "    match(id) {" << std::endl
           << "      0x1000 => aov_normals," << std::endl
           << "      0x1001 => aov_albedo," << std::endl
           << "      0x1002 => aov_depth," << std::endl
           << "      _ => make_empty_aov_image()" << std::endl
           << "    }" << std::endl
           << "  };" << std::endl;

    stream << "  let technique = make_infobuffer_renderer(" << max_depth << ", aovs, " << (handle_specular ? "true" : "false") << ");" << std::endl;

    return true;
}

static bool handle_ib_header(std::ostream& stream, const LoaderContext& ctx)
{
    const auto& info = ctx.TechniqueInfo;

    const auto& normal_it = std::find(info.EnabledAOVs.begin(), info.EnabledAOVs.end(), "Normals");
    if (normal_it == info.EnabledAOVs.end())
        return false;

    if (ctx.CurrentTechniqueVariant != info.Variants.size() - 1)
        return false;

    constexpr int C = 1 /* Depth */ + 1 /* Distance */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = init_ib_raypayload();" << std::endl;

    return true;
}

static TechniqueInfo ib_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    const bool apply_always = !ctx.Denoiser.OnlyFirstIteration || (technique ? technique->property("denoiser_ib_all_iterations").getBool(false) : false);

    TechniqueInfo info;
    enable_ib(info, apply_always, false);
    return info;
}

static void ib_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, LoaderContext& ctx)
{
    handle_ib_body(stream, technique, ctx);
}

static void ib_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    handle_ib_header(stream, ctx);
}

/////////////////////////

static TechniqueInfo path_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    TechniqueInfo info;

    // Check if we have a proper defined technique
    // It is totally fine to only define the type by other means then the scene config
    if (technique) {
        if (technique->property("aov_mis").getBool(false)) {
            info.EnabledAOVs.emplace_back("Direct Weights");
            info.EnabledAOVs.emplace_back("NEE Weights");
            info.Variants[0].ShadowHandlingMode = ShadowHandlingMode::Advanced;
        }
    }

    info.Variants[0].UsesLights = true;

    if (ctx.Denoiser.Enabled)
        enable_ib(info, !ctx.Denoiser.OnlyFirstIteration);

    return info;
}

static void path_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, LoaderContext& ctx)
{
    if (handle_ib_body(stream, technique, ctx))
        return;

    const int max_depth     = technique ? technique->property("max_depth").getInteger(64) : 64;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions
    const std::string ls    = technique ? technique->property("light_selector").getString(DefaultLightSelector) : DefaultLightSelector;
    const bool hasMISAOV    = technique ? technique->property("aov_mis").getBool(false) : false;

    if (hasMISAOV) {
        stream << "  let aov_di  = device.load_aov_image(\"Direct Weights\", spi); aov_di.mark_as_used();" << std::endl;
        stream << "  let aov_nee = device.load_aov_image(\"NEE Weights\", spi); aov_nee.mark_as_used();" << std::endl;
    }

    stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
           << "    match(id) {" << std::endl;

    if (hasMISAOV) {
        stream << "      1 => aov_di," << std::endl
               << "      2 => aov_nee," << std::endl;
    }

    stream << "      _ => make_empty_aov_image()" << std::endl
           << "    }" << std::endl
           << "  };" << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generateLightSelector(ls, tree);

    stream << "  let technique = make_path_renderer(" << max_depth << ", light_selector, aovs, " << clamp_value << ");" << std::endl;
}

static void path_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    if (handle_ib_header(stream, ctx))
        return;

    constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = init_pt_raypayload();" << std::endl;
}

/////////////////////////

static TechniqueInfo volpath_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true;
    info.Variants[0].UsesMedia  = true;

    if (ctx.Denoiser.Enabled)
        enable_ib(info, !ctx.Denoiser.OnlyFirstIteration);

    return info;
}

static void volpath_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, LoaderContext& ctx)
{
    if (handle_ib_body(stream, technique, ctx))
        return;

    const int max_depth     = technique ? technique->property("max_depth").getInteger(64) : 64;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions
    const std::string ls    = technique ? technique->property("light_selector").getString(DefaultLightSelector) : DefaultLightSelector;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generateLightSelector(ls, tree);

    stream << "  let aovs = @|_id:i32| make_empty_aov_image();" << std::endl;
    stream << "  let technique = make_volume_path_renderer(" << max_depth << ", light_selector, media, aovs, " << clamp_value << ");" << std::endl;
}

static void volpath_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    if (handle_ib_header(stream, ctx))
        return;

    constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */ + 1 /* Medium */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = init_vpt_raypayload();" << std::endl;
}

/////////////////////////

static std::string restir_resampling_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx, true) << std::endl;

    stream << "#[export] fn ig_callback_shader(settings: &Settings, iter: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    stream << "  resampling_pass(device, iter," << ctx.SamplesPerIteration << ", settings.frame,"
                                                                              ");"
           << std::endl
           << "}" << std::endl;

    return stream.str();
}

static TechniqueInfo restir_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    TechniqueInfo info;

    info.Variants[0].ShadowHandlingMode     = ShadowHandlingMode::Advanced;
    info.Variants[0].RequiresExplicitCamera = true;
    info.Variants[0].IsInteractive          = true;

    // make use of post-iteration setup
    info.Variants[0].CallbackGenerators[(int)CallbackType::AfterIteration] = restir_resampling_generator;

    // Check if we have a proper defined technique
    // It is totally fine to only define the type by other means then the scene config
    if (technique) {
        info.EnabledAOVs.emplace_back("ReSTIR");
        info.EnabledAOVs.emplace_back("Direct Light");
        info.EnabledAOVs.emplace_back("Depth Info");
    }

    info.Variants[0].UsesLights = true;

    if (ctx.Denoiser.Enabled)
        enable_ib(info, !ctx.Denoiser.OnlyFirstIteration);

    return info;
}

static void restir_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, LoaderContext& ctx)
{
    if (handle_ib_body(stream, technique, ctx))
        return;

    const int max_depth     = technique ? technique->property("max_depth").getInteger(4) : 4;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions
    const std::string ls    = technique ? technique->property("light_selector").getString(DefaultLightSelector) : DefaultLightSelector;

    stream << "  let aov_restir = device.load_aov_image(\"ReSTIR\", spi); aov_restir.mark_as_used();" << std::endl;
    stream << "  let aov_direct_light = device.load_aov_image(\"Direct Light\", spi); aov_direct_light.mark_as_used();" << std::endl;
    stream << "  let aov_depth_info = device.load_aov_image(\"Depth Info\", 1); aov_depth_info.mark_as_used();" << std::endl;

    stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
           << "    match(id) {" << std::endl;

    stream << "      1 => aov_restir," << std::endl;
    stream << "      2 => aov_direct_light," << std::endl;
    stream << "      3 => aov_depth_info," << std::endl;

    stream << "      _ => make_empty_aov_image()" << std::endl
           << "    }" << std::endl
           << "  };" << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generateLightSelector(ls, tree);

    stream << "  let technique = make_restir_renderer(camera, device," << max_depth << ", light_selector, aovs, " << clamp_value << ", settings.frame);" << std::endl;
}

static void restir_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    if (handle_ib_header(stream, ctx))
        return;

    constexpr int C = 1 /* INV_PDF */ + 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = init_rs_raypayload();" << std::endl;
}

/////////////////////////////////

static std::string ppm_light_camera_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << RayGenerationShader::begin(ctx) << std::endl;

    stream << ShaderUtils::generateDatabase() << std::endl;

    ShadingTree tree(ctx);
    stream << ctx.Lights->generate(tree, false) << std::endl;
    stream << ctx.Lights->generateLightSelector("", tree);

    stream << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl;
    stream << "  let emitter = make_ppm_light_emitter(light_selector, iter);" << std::endl;

    stream << RayGenerationShader::end();

    return stream.str();
}

static std::string ppm_before_iteration_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << ShaderUtils::beginCallback(ctx) << std::endl
           << "  let scene_bbox = " << LoaderUtils::inlineSceneBBox(ctx) << ";" << std::endl
           << "  ppm_handle_before_iteration(device, iter, " << ctx.CurrentTechniqueVariant << ", PPMPhotonCount, scene_bbox);" << std::endl
           << ShaderUtils::endCallback() << std::endl;

    return stream.str();
}

static TechniqueInfo ppm_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    TechniqueInfo info;

    // We got two passes. (0 -> Light Tracer, 1 -> Path Tracer with merging)
    info.Variants.resize(2);
    info.Variants[0].UsesLights = false; // LT makes no use of other lights (but starts on one)
    info.Variants[1].UsesLights = true;  // Standard PT still use of lights in the miss shader

    // To start from a light source, we do have to override the standard camera generator for LT
    info.Variants[0].OverrideCameraGenerator = ppm_light_camera_generator;

    // Each pass makes use of pre-iteration setups
    info.Variants[0].CallbackGenerators[(int)CallbackType::BeforeIteration] = ppm_before_iteration_generator; // Reset light cache
    info.Variants[1].CallbackGenerators[(int)CallbackType::BeforeIteration] = ppm_before_iteration_generator; // Construct query structure

    // The LT works independent of the framebuffer and requires a different work size
    const int max_photons           = technique ? technique->property("photons").getInteger(1000000) : 1000000;
    info.Variants[0].OverrideWidth  = max_photons; // Photon count
    info.Variants[0].OverrideHeight = 1;
    info.Variants[0].OverrideSPI    = 1; // The light tracer variant is always just one spi (Could be improved in the future though)

    info.Variants[0].LockFramebuffer = true; // We do not change the framebuffer

    // Check if we have a proper defined technique
    // It is totally fine to only define the type by other means then the scene config
    if (technique) {
        if (technique->property("aov").getBool(false)) {
            info.EnabledAOVs.emplace_back("Direct Weights");
            info.EnabledAOVs.emplace_back("Merging Weights");
        }
    }

    if (ctx.Denoiser.Enabled)
        enable_ib(info, !ctx.Denoiser.OnlyFirstIteration);

    return info;
}

static void ppm_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, LoaderContext& ctx)
{
    if (handle_ib_body(stream, technique, ctx))
        return;

    const int max_depth     = technique ? technique->property("max_depth").getInteger(8) : 8;
    const float radius      = technique ? technique->property("radius").getNumber(0.01f) : 0.01f;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions

    bool is_lighttracer = ctx.CurrentTechniqueVariant == 0;

    if (is_lighttracer) {
        stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
               << "    match(id) {" << std::endl
               << "      _ => make_empty_aov_image()" << std::endl
               << "    }" << std::endl
               << "  };" << std::endl;
    } else {
        const bool hasAOV = technique ? technique->property("aov").getBool(false) : false;

        if (hasAOV) {
            stream << "  let aov_di   = device.load_aov_image(\"Direct Weights\", spi); aov_di.mark_as_used();" << std::endl;
            stream << "  let aov_merg = device.load_aov_image(\"Merging Weights\", spi); aov_merg.mark_as_used();" << std::endl;
        }

        stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
               << "    match(id) {" << std::endl;

        if (hasAOV) {
            stream << "      1 => aov_di," << std::endl
                   << "      2 => aov_merg," << std::endl;
        }

        stream << "      _ => make_empty_aov_image()" << std::endl
               << "    }" << std::endl
               << "  };" << std::endl;

        stream << "  let ppm_radius = ppm_compute_radius(" << radius * ctx.Environment.SceneDiameter << ", settings.iter);" << std::endl;
    }

    stream << "  let scene_bbox  = " << LoaderUtils::inlineSceneBBox(ctx) << ";" << std::endl
           << "  let light_cache = make_ppm_lightcache(device, PPMPhotonCount, scene_bbox);" << std::endl;

    if (is_lighttracer) {
        stream << "  let technique = make_ppm_light_renderer(" << max_depth << ", aovs, light_cache);" << std::endl;
    } else {
        ShadingTree tree(ctx);
        stream << ctx.Lights->generateLightSelector("", tree);
        stream << "  let technique = make_ppm_path_renderer(" << max_depth << ",light_selector, ppm_radius, aovs, " << clamp_value << ", light_cache);" << std::endl;
    }
}

static void ppm_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    if (handle_ib_header(stream, ctx))
        return;

    constexpr int C          = 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */ + 1 /* Light/Radius */ + 1 /* PathType */;
    const size_t max_photons = std::max(100, technique ? technique->property("photons").getInteger(1000000) : 1000000);

    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = init_ppm_raypayload();" << std::endl
           << "static PPMPhotonCount = " << max_photons << ":i32;" << std::endl;
}

/////////////////////////////////

// Will return information about the enabled AOVs
using TechniqueGetInfo = TechniqueInfo (*)(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

// Every body loader has to define 'technique'
using TechniqueBodyLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, LoaderContext&);

// Every header loader has to define 'RayPayloadComponents' and 'init_raypayload()'
using TechniqueHeaderLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

static const struct TechniqueEntry {
    const char* Name;
    TechniqueGetInfo GetInfo;
    TechniqueBodyLoader BodyLoader;
    TechniqueHeaderLoader HeaderLoader;
} _generators[] = {
    { "ao", technique_empty_get_info, ao_body_loader, technique_empty_header_loader },
    { "path", path_get_info, path_body_loader, path_header_loader },
    { "volpath", volpath_get_info, volpath_body_loader, volpath_header_loader },
    { "debug", debug_get_info, debug_body_loader, technique_empty_header_loader },
    { "ppm", ppm_get_info, ppm_body_loader, ppm_header_loader },
    { "photonmapper", ppm_get_info, ppm_body_loader, ppm_header_loader },
    { "wireframe", wireframe_get_info, wireframe_body_loader, wireframe_header_loader },
    { "infobuffer", ib_get_info, ib_body_loader, ib_header_loader },
    { "restir", restir_get_info, restir_body_loader, restir_header_loader },
    { "", nullptr, nullptr, nullptr }
};

static const TechniqueEntry* getTechniqueEntry(const std::string& name)
{
    const std::string lower_name = to_lowercase(name);
    for (size_t i = 0; _generators[i].HeaderLoader; ++i) {
        if (_generators[i].Name == lower_name)
            return &_generators[i];
    }
    IG_LOG(L_ERROR) << "No technique type '" << name << "' available" << std::endl;
    return nullptr;
}

std::optional<TechniqueInfo> LoaderTechnique::getInfo(const LoaderContext& ctx)
{
    const auto* entry = getTechniqueEntry(ctx.TechniqueType);
    if (!entry)
        return {};

    const auto technique = ctx.Scene.technique();

    return entry->GetInfo(ctx.TechniqueType, technique, ctx);
}

std::string LoaderTechnique::generate(LoaderContext& ctx)
{
    const auto* entry = getTechniqueEntry(ctx.TechniqueType);
    if (!entry)
        return {};

    const auto technique = ctx.Scene.technique();

    std::stringstream stream;
    entry->BodyLoader(stream, ctx.TechniqueType, technique, ctx);

    return stream.str();
}

std::string LoaderTechnique::generateHeader(const LoaderContext& ctx, bool isRayGeneration)
{
    IG_UNUSED(isRayGeneration);

    const auto* entry = getTechniqueEntry(ctx.TechniqueType);
    if (!entry)
        return {};

    const auto technique = ctx.Scene.technique();

    std::stringstream stream;
    entry->HeaderLoader(stream, ctx.TechniqueType, technique, ctx);

    return stream.str();
}

std::vector<std::string> LoaderTechnique::getAvailableTypes()
{
    std::vector<std::string> array;

    for (size_t i = 0; _generators[i].HeaderLoader; ++i)
        array.emplace_back(_generators[i].Name);

    std::sort(array.begin(), array.end());
    return array;
}
} // namespace IG