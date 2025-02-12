#include "HitShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/Loader.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderMedium.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"
#include "loader/ShadingTree.h"

#include <sstream>

namespace IG {
std::string HitShader::setup(size_t mat_id, LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_material_shader(settings: &Settings, mat_id: i32, first: i32, last: i32) -> () {" << std::endl
           << ShaderUtils::constructDevice(ctx.Options) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << "  let scene_bbox = " << ShaderUtils::inlineSceneBBox(ctx) << "; maybe_unused(scene_bbox);" << std::endl
           << std::endl;

    stream << ShaderUtils::generateDatabase(ctx) << std::endl;
    stream << ShaderUtils::inlineScene(ctx, false);

    ShadingTree tree(ctx);
    const bool requireLights = ctx.Technique->info().UsesLights;
    if (requireLights)
        stream << ctx.Lights->generate(tree, false) << std::endl;

    const bool requireMedia = ctx.Technique->info().UsesMedia;
    if (requireMedia)
        stream << ctx.Media->generate(tree) << std::endl;

    stream << ShaderUtils::generateMaterialShader(tree, mat_id, requireLights, "shader") << std::endl;

    // Include camera if necessary
    if (ctx.Technique->info().RequiresExplicitCamera)
        stream << ctx.Camera->generate(tree) << std::endl;

    // Will define technique
    stream << ctx.Technique->generate(tree) << std::endl
           << std::endl;

    stream << "  device.handle_hit_shader(shader, scene, full_technique, payload_info, first, last);" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG