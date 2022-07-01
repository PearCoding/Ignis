#include "ShaderUtils.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"

#include <cctype>
#include <sstream>

namespace IG {
std::string ShaderUtils::constructDevice(Target target)
{
    std::stringstream stream;

    switch (target) {
    case Target::AVX:
        stream << "let device = make_avx_device();";
        break;
    case Target::AVX2:
        stream << "let device = make_avx2_device();";
        break;
    case Target::AVX512:
        stream << "let device = make_avx512_device();";
        break;
    case Target::SSE42:
        stream << "let device = make_sse42_device();";
        break;
    case Target::ASIMD:
        stream << "let device = make_asimd_device();";
        break;
    case Target::NVVM:
        stream << "let device = make_nvvm_device(settings.device);";
        break;
    case Target::AMDGPU:
        stream << "let device = make_amdgpu_device(settings.device);";
        break;
    default:
        stream << "let device = make_cpu_default_device();";
        break;
    }

    return stream.str();
}

std::string ShaderUtils::generateDatabase()
{
    std::stringstream stream;
    stream << "  let dtb      = device.load_scene_database();" << std::endl
           << "  let shapes   = device.load_shape_table(dtb.shapes); maybe_unused(shapes);" << std::endl
           << "  let entities = device.load_entity_table(dtb.entities); maybe_unused(entities);" << std::endl;
    return stream.str();
}

std::string ShaderUtils::generateMaterialShader(ShadingTree& tree, size_t mat_id, bool requireLights, const std::string_view& output_var)
{
    std::stringstream stream;

    const Material material = tree.context().Environment.Materials.at(mat_id);
    stream << LoaderBSDF::generate(material.BSDF, tree);

    std::string bsdf_id = tree.getClosureID(material.BSDF);
    const bool isLight  = material.hasEmission() && tree.context().Lights->isAreaLight(material.Entity);

    if (material.hasMediumInterface())
        stream << "  let medium_interface = make_medium_interface(" << material.MediumInner << ", " << material.MediumOuter << ");" << std::endl;
    else
        stream << "  let medium_interface = no_medium_interface();" << std::endl;

    // We do not embed the actual material id into the shader, as this makes the shader unique without any major performance gain
    if (isLight && requireLights) {
        const size_t light_id = tree.context().Lights->getAreaLightID(material.Entity);
        stream << "  let " << output_var << " : MaterialShader = @|ctx| make_emissive_material(mat_id, ctx.surf, bsdf_" << bsdf_id << "(ctx), medium_interface,"
               << " @lights(" << light_id << "));" << std::endl
               << std::endl;
    } else {
        stream << "  let " << output_var << " : MaterialShader = @|ctx| make_material(mat_id, bsdf_" << bsdf_id << "(ctx), medium_interface);" << std::endl
               << std::endl;
    }

    return stream.str();
}

std::string ShaderUtils::beginCallback(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << LoaderTechnique::generateHeader(ctx, true) << std::endl;

    stream << "#[export] fn ig_callback_shader(settings: &Settings, iter: i32) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;
    return stream.str();
}

std::string ShaderUtils::endCallback()
{
    return "}";
}

std::string ShaderUtils::inlineSPI(const LoaderContext& ctx)
{
    std::stringstream stream;

    if (ctx.SamplesPerIteration == 1) // Hardcode this case as some optimizations might apply
        stream << ctx.SamplesPerIteration << " : i32";
    else // Fallback to dynamic spi
        stream << "registry::get_global_parameter_i32(\"__spi\", 1)";

    // We do not hardcode the spi as default to prevent recompilations if spi != 1
    return stream.str();
}
} // namespace IG