#include "RayGenerationShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/Loader.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string RayGenerationShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_ray_generation_shader(settings: &Settings, iter: i32, id: &mut i32, size: i32, xmin: i32, ymin: i32, xmax: i32, ymax: i32) -> i32 {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;

    return stream.str();
}

std::string RayGenerationShader::end(const std::string_view emitterName, const std::string_view spiName, bool skipReturn)
{
    std::stringstream stream;

    if (!skipReturn)
        stream << "  device.generate_rays(" << emitterName << ", id, size, xmin, ymin, xmax, ymax, " << spiName << ")" << std::endl;

    stream << "}" << std::endl;

    return stream.str();
}

std::string RayGenerationShader::setup(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << begin(ctx) << std::endl
           << std::endl
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl;

    if (ctx.IsTracer) {
        stream << "  let emitter = make_list_emitter(device.load_rays(), iter, " << ctx.CurrentTechniqueVariantInfo().GetEmitterPayloadInitializer() << "());" << std::endl;
    } else {
        stream << LoaderCamera::generate(ctx) << std::endl; // Will set `camera`

        std::string pixel_sampler = "make_uniform_pixel_sampler()";
        if (ctx.PixelSamplerType == "halton") {
            stream << "  let halton_setup = setup_halton_pixel_sampler(device, settings.width, settings.height, iter, xmin, ymin, xmax, ymax);" << std::endl;
            pixel_sampler = "make_halton_pixel_sampler(halton_setup)";
        } else if (ctx.PixelSamplerType == "mjitt") {
            pixel_sampler = "make_mjitt_pixel_sampler(4, 4)";
        }

        stream << "  let emitter = make_camera_emitter(camera, iter, spi, " << pixel_sampler << ", " << ctx.CurrentTechniqueVariantInfo().GetEmitterPayloadInitializer() << "());" << std::endl;
    }

    stream << end();

    return stream.str();
}

} // namespace IG