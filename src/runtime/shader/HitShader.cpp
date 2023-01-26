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
    
    stream << "#[export] fn ig_hit_shader(settings: &Settings, mat_id: i32, first: i32, last: i32) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << "  let scene_bbox = " << ShaderUtils::inlineSceneBBox(ctx) << "; maybe_unused(scene_bbox);" << std::endl
           << std::endl;

    stream << ShaderUtils::generateDatabase(ctx) << std::endl;

    stream << "  let scene  = Scene {" << std::endl
           << "    info     = " << ShaderUtils::inlineSceneInfo(ctx, false) << "," << std::endl
           << "    shapes   = shapes," << std::endl
           << "    entities = entities," << std::endl
           << "  };" << std::endl
           << std::endl;

    ShadingTree tree(ctx);
    const bool requireLights = ctx.CurrentTechniqueVariantInfo().UsesLights;
    if (requireLights)
        stream << ctx.Lights->generate(tree, false) << std::endl;

    const bool requireMedia = ctx.CurrentTechniqueVariantInfo().UsesMedia;
    if (requireMedia)
        stream << ctx.Media->generate(tree) << std::endl;

    stream << ShaderUtils::generateMaterialShader(tree, mat_id, requireLights, "shader") << std::endl;

    // Include camera if necessary
    if (ctx.CurrentTechniqueVariantInfo().RequiresExplicitCamera)
        stream << ctx.Camera->generate(ctx) << std::endl;

    // Will define technique
    stream << ctx.Technique->generate(ctx) << std::endl
           << std::endl;

    stream << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
           << "  device.handle_hit_shader(shader, scene, technique, payload_info, first, last, use_framebuffer);" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG