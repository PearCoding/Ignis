#include "MissShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/Loader.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderMedium.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string MissShader::setup(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_miss_shader(settings: &Settings, first: i32, last: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << std::endl;

    ShadingTree tree(ctx);

    if ((ctx.CurrentTechniqueVariantInfo().UsesLights && ctx.CurrentTechniqueVariantInfo().UsesAllLightsInMiss)
        || ctx.CurrentTechniqueVariantInfo().UsesMedia)
        stream << ShaderUtils::generateDatabase(ctx) << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().UsesLights)
        stream << ctx.Lights->generate(tree, !ctx.CurrentTechniqueVariantInfo().UsesAllLightsInMiss)
               << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().UsesMedia)
        stream << ctx.Media->generate(tree) << std::endl;

    // Include camera if necessary
    if (ctx.CurrentTechniqueVariantInfo().RequiresExplicitCamera)
        stream << ctx.Camera->generate(ctx) << std::endl;

    stream << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl;

    // Will define technique
    stream << ctx.Technique->generate(ctx) << std::endl
           << std::endl;

    stream << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
           << "  device.handle_miss_shader(technique, payload_info, first, last, spi, use_framebuffer)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG