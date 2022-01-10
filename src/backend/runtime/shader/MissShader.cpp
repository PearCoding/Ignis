#include "MissShader.h"
#include "Logger.h"
#include "loader/Loader.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderTechnique.h"
#include "loader/ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string MissShader::setup(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx) << std::endl;

    stream << "#[export] fn ig_miss_shader(settings: &Settings, first: i32, last: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    if (ctx.TechniqueInfo.UsesLights[ctx.CurrentTechniqueVariant]) {
        if (ctx.TechniqueInfo.UsesAllLightsInMiss[ctx.CurrentTechniqueVariant])
            stream << ShaderUtils::generateDatabase() << std::endl;

        stream << LoaderLight::generate(ctx, !ctx.TechniqueInfo.UsesAllLightsInMiss[ctx.CurrentTechniqueVariant])
               << std::endl;
    }

    stream << "  let spp = " << ctx.SamplesPerIteration << " : i32;" << std::endl;

    // Will define technique
    stream << LoaderTechnique::generate(ctx) << std::endl
           << std::endl;

    stream << "  device.handle_miss_shader(technique, first, last, spp)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG