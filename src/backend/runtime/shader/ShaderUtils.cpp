#include "ShaderUtils.h"
#include "loader/LoaderBSDF.h"
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

    const bool isLight = material.hasEmission() && tree.context().Environment.AreaLightsMap.count(material.Entity) > 0;

    if (material.hasMediumInterface())
        stream << "  let medium_interface = make_medium_interface(" << material.MediumInner << ", " << material.MediumOuter << ");" << std::endl;
    else
        stream << "  let medium_interface = no_medium_interface();" << std::endl;

    if (isLight && requireLights) {
        const uint32 light_id = tree.context().Environment.AreaLightsMap.at(material.Entity);
        stream << "  let " << output_var << " : Shader = @|ray, hit, surf| make_emissive_material(" << mat_id << ", surf, bsdf_" << LoaderUtils::escapeIdentifier(material.BSDF) << "(ray, hit, surf), medium_interface,"
               << " @lights(" << light_id << "));" << std::endl
               << std::endl;
    } else {
        stream << "  let " << output_var << " : Shader = @|ray, hit, surf| make_material(" << mat_id << ", bsdf_" << LoaderUtils::escapeIdentifier(material.BSDF) << "(ray, hit, surf), medium_interface);" << std::endl
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
} // namespace IG