#include "HitShader.h"
#include "Logger.h"
#include "ShaderBuilder.h"
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
ShaderBuilder HitShader::setup(size_t mat_id, LoaderContext& ctx)
{
    ShaderBuilder builder;

    builder.merge(ShaderUtils::constructDevice(ctx))
        .addStatement("let payload_info = " + ShaderUtils::inlinePayloadInfo(ctx) + ";")
        .merge(ShaderUtils::generateSceneBBox(ctx))
        .merge(ShaderUtils::generateDatabase(ctx))
        .merge(ShaderUtils::generateScene(ctx, false));

    ShadingTree tree(ctx);
    const bool requireLights = ctx.CurrentTechniqueVariantInfo().UsesLights;
    if (requireLights)
        builder.merge(ctx.Lights->generate(tree, false));

    const bool requireMedia = ctx.CurrentTechniqueVariantInfo().UsesMedia;
    if (requireMedia)
        builder.merge(ctx.Media->generate(tree));

    builder.merge(ShaderUtils::generateMaterialShader(tree, mat_id, requireLights, "shader"));

    // Include camera if necessary
    if (ctx.CurrentTechniqueVariantInfo().RequiresExplicitCamera)
        builder.merge(ctx.Camera->generate(ctx));

    // Will define technique
    builder.merge(ctx.Technique->generate(ctx))
        .addStatement("let use_framebuffer = " + std::string(!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") + ";")
        .addStatement("device.handle_hit_shader(shader, scene, full_technique, payload_info, first, last, use_framebuffer);");

    ShaderBuilder topBuild;
    topBuild.addFunction("#[export] fn ig_hit_shader(settings: &Settings, mat_id: i32, first: i32, last: i32) -> ()", builder);
    return topBuild;
}

} // namespace IG