#include "AdvancedShadowShader.h"
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
using namespace Parser;

std::string AdvancedShadowShader::setup(bool is_hit, size_t mat_id, LoaderContext& ctx)
{
    std::stringstream stream;

    ShadingTree tree(ctx);

    stream << "#[export] fn ig_advanced_shadow_shader(settings: &Settings, mat_id: i32, first: i32, last: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl
           << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().UsesLights) {
        bool requireAreaLight = is_hit || ctx.CurrentTechniqueVariantInfo().UsesAllLightsInMiss;
        if (requireAreaLight)
            stream << ShaderUtils::generateDatabase(ctx) << std::endl;

        stream << ctx.Lights->generate(tree, !requireAreaLight)
               << std::endl;
    }

    if (ctx.CurrentTechniqueVariantInfo().UsesMedia)
        stream << ctx.Media->generate(tree) << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().ShadowHandlingMode == ShadowHandlingMode::AdvancedWithMaterials) {
        stream << ShaderUtils::generateMaterialShader(tree, mat_id, ctx.CurrentTechniqueVariantInfo().UsesLights, "shader") << std::endl;
    } else {
        stream << "  let shader : MaterialShader = @|ctx| make_material(mat_id, make_black_bsdf(ctx.surf), no_medium_interface());" << std::endl
               << std::endl;
    }

    // Include camera if necessary
    if (ctx.CurrentTechniqueVariantInfo().RequiresExplicitCamera)
        stream << ctx.Camera->generate(ctx) << std::endl;

    stream << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl;

    // Will define technique
    stream << ctx.Technique->generate(ctx) << std::endl
           << std::endl;

    stream << "  let is_hit = " << (is_hit ? "true" : "false") << ";" << std::endl
           << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
           << "  device.handle_advanced_shadow_shader(shader, technique, payload_info, first, last, spi, use_framebuffer, is_hit)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG