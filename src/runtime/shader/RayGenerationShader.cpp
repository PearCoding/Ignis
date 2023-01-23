#include "RayGenerationShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/Loader.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"

#include <sstream>

namespace IG {
std::string RayGenerationShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_ray_generation_shader(settings: &Settings, next_id: i32, size: i32, xmin: i32, ymin: i32, xmax: i32, ymax: i32) -> i32 {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << "  let scene_bbox = " << ShaderUtils::inlineSceneBBox(ctx) << "; maybe_unused(scene_bbox);" << std::endl;

    return stream.str();
}

std::string RayGenerationShader::end(const std::string_view& emitterName, const std::string_view& spiName, bool skipReturn)
{
    std::stringstream stream;

    if (!skipReturn)
        stream << "  device.generate_rays(" << emitterName << ", payload_info, next_id, size, xmin, ymin, xmax, ymax, " << spiName << ")" << std::endl;

    stream << "}" << std::endl;

    return stream.str();
}

std::string RayGenerationShader::generatePixelSampler(const LoaderContext& ctx, const std::string_view& varName)
{
    std::stringstream stream;

    if (ctx.Options.PixelSamplerType == "halton") {
        stream << "  let halton_setup = setup_halton_pixel_sampler(device, settings.width, settings.height, settings.iter, xmin, ymin, xmax, ymax);" << std::endl
               << "  let " << varName << " = make_halton_pixel_sampler(halton_setup);" << std::endl;
    } else if (ctx.Options.PixelSamplerType == "mjitt") {
        stream << "  let " << varName << " = make_mjitt_pixel_sampler(4, 4);" << std::endl;
    } else {
        stream << "  let " << varName << " = make_uniform_pixel_sampler();" << std::endl;
    }

    return stream.str();
}

std::string RayGenerationShader::setup(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << begin(ctx) << std::endl
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  let init_raypayload = " << ctx.CurrentTechniqueVariantInfo().GetEmitterPayloadInitializer() << ";" << std::endl;

    if (ctx.Options.IsTracer) {
        stream << "  let emitter = make_list_emitter(device.load_rays(), settings.iter, settings.seed, init_raypayload);" << std::endl;
    } else {
        stream << ctx.Camera->generate(ctx) << std::endl // Will set `camera`
               << generatePixelSampler(ctx) << std::endl // Will set `pixel_sampler`
               << "  let emitter = make_camera_emitter(camera, settings.iter, spi, settings.frame, settings.seed, pixel_sampler, init_raypayload);" << std::endl;
    }

    stream << end();

    return stream.str();
}

} // namespace IG