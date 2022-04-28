#include "LoaderTechnique.h"
#include "Loader.h"
#include "LoaderLight.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"
#include "serialization/VectorSerializer.h"

namespace IG {

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

static void ao_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    stream << "  let technique = make_ao_renderer();" << std::endl;
}

/////////////////////////

static void debug_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    // TODO: Maybe add a changeable default mode?
    stream << "  let debug_mode = registry::get_parameter_i32(\"__debug_mode\", 0);" << std::endl
           << "  maybe_unused(num_lights); maybe_unused(lights);" << std::endl
           << "  let technique  = make_debug_renderer(debug_mode);" << std::endl;
}

static TechniqueInfo debug_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true; // We make use of the emissive information!
    return info;
}

/////////////////////////

static void wireframe_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
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

static TechniqueInfo path_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
    TechniqueInfo info;

    // Check if we have a proper defined technique
    // It is totally fine to only define the type by other means then the scene config
    if (technique) {
        if (technique->property("aov_normals").getBool(false))
            info.EnabledAOVs.emplace_back("Normals");

        if (technique->property("aov_mis").getBool(false)) {
            info.EnabledAOVs.emplace_back("Direct Weights");
            info.EnabledAOVs.emplace_back("NEE Weights");
            info.Variants[0].UseAdvancedShadowHandling = true;
        }
    }

    info.Variants[0].UsesLights = true;

    return info;
}

static void path_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
    const int max_depth     = technique ? technique->property("max_depth").getInteger(64) : 64;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions
    const bool hasNormalAOV = technique ? technique->property("aov_normals").getBool(false) : false;
    const bool hasMISAOV    = technique ? technique->property("aov_mis").getBool(false) : false;

    size_t counter = 1;
    if (hasNormalAOV)
        stream << "  let aov_normals = device.load_aov_image(" << counter++ << ", spp);" << std::endl;

    if (hasMISAOV) {
        stream << "  let aov_di = device.load_aov_image(" << counter++ << ", spp);" << std::endl;
        stream << "  let aov_nee = device.load_aov_image(" << counter++ << ", spp);" << std::endl;
    }

    stream << "  let aovs = @|id:i32| -> AOVImage {" << std::endl
           << "    match(id) {" << std::endl;

    // TODO: We do not support constants in match (or any other useful location)!!!!!!!!!
    // This is completely unnecessary... we have to fix artic for that......
    if (hasNormalAOV)
        stream << "      1 => aov_normals," << std::endl;

    if (hasMISAOV) {
        stream << "      2 => aov_di," << std::endl
               << "      3 => aov_nee," << std::endl;
    }

    stream << "      _ => make_empty_aov_image()" << std::endl
           << "    }" << std::endl
           << "  };" << std::endl;

    stream << "  let technique = make_path_renderer(" << max_depth << ", num_lights, lights, aovs, " << clamp_value << ");" << std::endl;
}

static void path_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = wrap_ptraypayload(PTRayPayload { mis = 0, contrib = color_builtins::white, depth = 1, eta = 1 });" << std::endl;
}

/////////////////////////

static TechniqueInfo volpath_get_info(const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true;
    info.Variants[0].UsesMedia  = true;

    return info;
}

static void volpath_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
    const int max_depth     = technique ? technique->property("max_depth").getInteger(64) : 64;
    const float clamp_value = technique ? technique->property("clamp").getNumber(0) : 0; // Allow clamping of contributions


    stream << "  let aovs = @|_id:i32| make_empty_aov_image();" << std::endl;
    stream << "  let technique = make_volume_path_renderer(" << max_depth << ", num_lights, lights, media, aovs, " << clamp_value << ");" << std::endl;
}

static void volpath_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&)
{
    constexpr int C = 1 /* MIS */ + 3 /* Contrib */ + 1 /* Depth */ + 1 /* Eta */ + 1 /* Medium */;
    stream << "static RayPayloadComponents = " << C << ";" << std::endl
           << "fn init_raypayload() = wrap_vptraypayload(VPTRayPayload { mis = 0, contrib = color_builtins::white, depth = 1, eta = 1, medium = -1 });" << std::endl;
}

/////////////////////////////////

static std::string ppm_light_camera_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx, true) << std::endl;

    stream << "#[export] fn ig_ray_generation_shader(settings: &Settings, iter: i32, id: &mut i32, size: i32, xmin: i32, ymin: i32, xmax: i32, ymax: i32) -> i32 {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    stream << ShaderUtils::generateDatabase() << std::endl;

    ShadingTree tree(ctx);
    stream << LoaderLight::generate(tree, false) << std::endl;

    stream << "  let spp = " << ctx.SamplesPerIteration << " : i32;" << std::endl;
    stream << "  let emitter = make_ppm_light_emitter(num_lights, lights, iter);" << std::endl;

    stream << "  device.generate_rays(emitter, id, size, xmin, ymin, xmax, ymax, spp)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

static std::string ppm_before_iteration_generator(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx, true) << std::endl;

    stream << "#[export] fn ig_callback_shader(settings: &Settings, iter: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    stream << "  let scene_bbox = " << ShaderUtils::inlineSceneBBox(ctx) << ";" << std::endl
           << "  ppm_handle_before_iteration(device, iter, " << ctx.CurrentTechniqueVariant << ", PPMPhotonCount, scene_bbox);" << std::endl
           << "}" << std::endl;

    return stream.str();
}

static TechniqueInfo ppm_get_info(const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
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

    return info;
}

static void ppm_body_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext& ctx)
{
    const int max_depth = technique ? technique->property("max_depth").getInteger(8) : 8;
    const float radius  = technique ? technique->property("radius").getNumber(0.01f) : 0.01f;
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
            stream << "  let aov_di   = device.load_aov_image(1, spp);" << std::endl;
            stream << "  let aov_merg = device.load_aov_image(2, spp);" << std::endl;
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

        stream << "  let ppm_radius = ppm_compute_radius(" << radius << ", settings.iter);" << std::endl;
    }

    stream << "  let scene_bbox  = " << ShaderUtils::inlineSceneBBox(ctx) << ";" << std::endl
           << "  let light_cache = make_ppm_lightcache(device, PPMPhotonCount, scene_bbox);" << std::endl;

    if (is_lighttracer)
        stream << "  let technique = make_ppm_light_renderer(" << max_depth << ", aovs, light_cache);" << std::endl;
    else
        stream << "  let technique = make_ppm_path_renderer(" << max_depth << ", num_lights, lights, ppm_radius, aovs, light_cache);" << std::endl;
}

static void ppm_header_loader(std::ostream& stream, const std::string&, const std::shared_ptr<Parser::Object>& technique, const LoaderContext&)
{
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
using TechniqueBodyLoader = void (*)(std::ostream&, const std::string&, const std::shared_ptr<Parser::Object>&, const LoaderContext&);

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

TechniqueInfo LoaderTechnique::getInfo(const LoaderContext& ctx)
{
    const auto* entry = getTechniqueEntry(ctx.TechniqueType);
    if (!entry)
        return {};

    const auto technique = ctx.Scene.technique();

    return entry->GetInfo(ctx.TechniqueType, technique, ctx);
}

std::string LoaderTechnique::generate(const LoaderContext& ctx)
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
} // namespace IG