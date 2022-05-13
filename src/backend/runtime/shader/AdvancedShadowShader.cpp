#include "AdvancedShadowShader.h"
#include "Logger.h"
#include "loader/Loader.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderCamera.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderMedium.h"
#include "loader/LoaderTechnique.h"
#include "loader/ShaderUtils.h"
#include "loader/ShadingTree.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string AdvancedShadowShader::setup(bool is_hit, size_t mat_id, LoaderContext& ctx)
{
    std::stringstream stream;

    ShadingTree tree(ctx);

    stream << LoaderTechnique::generateHeader(ctx) << std::endl;

    stream << "#[export] fn ig_advanced_shadow_shader(settings: &Settings, first: i32, last: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().UsesLights) {
        bool requireAreaLight = is_hit || ctx.CurrentTechniqueVariantInfo().UsesAllLightsInMiss;
        if (requireAreaLight)
            stream << ShaderUtils::generateDatabase() << std::endl;

        stream << LoaderLight::generate(tree, !requireAreaLight)
               << std::endl;
    }

    if (ctx.CurrentTechniqueVariantInfo().UsesMedia)
        stream << LoaderMedium::generate(tree) << std::endl;

    if (ctx.CurrentTechniqueVariantInfo().ShadowHandlingMode == ShadowHandlingMode::AdvancedWithMaterials) {
        const Material material = ctx.Environment.Materials.at(mat_id);
        stream << LoaderBSDF::generate(material.BSDF, tree);

        const bool isLight = material.hasEmission() && ctx.Environment.AreaLightsMap.count(material.Entity) > 0;

        if (material.hasMediumInterface())
            stream << "  let medium_interface = make_medium_interface(" << material.MediumInner << ", " << material.MediumOuter << ");" << std::endl;
        else
            stream << "  let medium_interface = no_medium_interface();" << std::endl;

        if (isLight && ctx.CurrentTechniqueVariantInfo().UsesLights) {
            const uint32 light_id = ctx.Environment.AreaLightsMap.at(material.Entity);
            stream << "  let shader : Shader = @|ray, hit, surf| make_emissive_material(" << mat_id << ", surf, bsdf_" << ShaderUtils::escapeIdentifier(material.BSDF) << "(ray, hit, surf), medium_interface,"
                   << " @lights(" << light_id << "));" << std::endl
                   << std::endl;
        } else {
            stream << "  let shader : Shader = @|ray, hit, surf| make_material(" << mat_id << ", bsdf_" << ShaderUtils::escapeIdentifier(material.BSDF) << "(ray, hit, surf), medium_interface);" << std::endl
                   << std::endl;
        }
    } else {
        stream << "  let shader : Shader = @|_, _, surf| make_material(" << mat_id << ", make_black_bsdf(surf), no_medium_interface());" << std::endl
               << std::endl;
    }

    // Include camera if necessary
    if (ctx.CurrentTechniqueVariantInfo().RequiresExplicitCamera)
        stream << LoaderCamera::generate(ctx) << std::endl;

    stream << "  let spp = " << ctx.SamplesPerIteration << " : i32;" << std::endl;

    // Will define technique
    stream << LoaderTechnique::generate(ctx) << std::endl
           << std::endl;

    stream << "  let is_hit = " << (is_hit ? "true" : "false") << ";" << std::endl
           << "  let use_framebuffer = " << (!ctx.CurrentTechniqueVariantInfo().LockFramebuffer ? "true" : "false") << ";" << std::endl
           << "  device.handle_advanced_shadow_shader(shader, technique, first, last, spp, use_framebuffer, is_hit)" << std::endl
           << "}" << std::endl;

    return stream.str();
}

} // namespace IG