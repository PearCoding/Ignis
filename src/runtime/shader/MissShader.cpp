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
std::string MissShader::setup(LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_material_shader(settings: &Settings, _mat_id: i32, first: i32, last: i32) -> () {" << std::endl
           << ShaderUtils::constructDevice(ctx.Options) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << "  let scene_bbox = " << ShaderUtils::inlineSceneBBox(ctx) << "; maybe_unused(scene_bbox);" << std::endl
           << std::endl;

    ShadingTree tree(ctx);

    if ((ctx.Technique->info().UsesLights && ctx.Technique->info().UsesAllLightsInMiss)
        || ctx.Technique->info().UsesMedia)
        stream << ShaderUtils::generateDatabase(ctx) << std::endl;

    if (ctx.Technique->info().UsesLights)
        stream << ctx.Lights->generate(tree, !ctx.Technique->info().UsesAllLightsInMiss)
               << std::endl;

    if (ctx.Technique->info().UsesMedia)
        stream << ctx.Media->generate(tree) << std::endl;

    // Include camera if necessary
    if (ctx.Technique->info().RequiresExplicitCamera)
        stream << ctx.Camera->generate(tree) << std::endl;

    // Will define technique
    stream << ctx.Technique->generate(tree) << std::endl
           << std::endl;

    stream << "  device.handle_miss_shader(full_technique, payload_info, first, last)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG