#include "ShaderUtils.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderLight.h"
#include "loader/LoaderShape.h"
#include "loader/LoaderTechnique.h"
#include "loader/LoaderUtils.h"

#include <cctype>
#include <sstream>

namespace IG {
std::string ShaderUtils::constructDevice(const Target& target)
{
    std::stringstream stream;

    stream << "let device = ";
    if (target.isCPU()) {
        const bool compact = false; /*target.vectorWidth() >= 8;*/ // FIXME: Maybe something wrong with this flag?
        const bool single  = target.vectorWidth() >= 4;

        // TODO: Better decisions?
        std::string min_max = "make_default_min_max()";
        if (target.vectorWidth() >= 4)
            min_max = "make_cpu_int_min_max()";

        stream << "make_cpu_device("
               << (compact ? "true" : "false") << ", "
               << (single ? "true" : "false") << ", "
               << min_max << ", "
               << target.vectorWidth()
               << ", settings.thread_count"
               << ", 16"
               << ", true);";
    } else {
        switch (target.gpuVendor()) {
        case GPUVendor::AMD:
            stream << "make_amdgpu_device(settings.device);";
            break;
        default:
        case GPUVendor::Nvidia:
            stream << "make_nvvm_device(settings.device);";
            break;
        }
    }

    return stream.str();
}

std::string ShaderUtils::generateDatabase(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << "  let entities = load_entity_table(device); maybe_unused(entities);" << std::endl
           << generateShapeLookup(ctx)
           << "  maybe_unused(shapes);" << std::endl;
    return stream.str();
}

std::string ShaderUtils::generateShapeLookup(const LoaderContext& ctx)
{
    std::vector<ShapeProvider*> provs;
    provs.reserve(ctx.Shapes->providers().size());
    for (const auto& p : ctx.Shapes->providers())
        provs.emplace_back(p.second.get());

    if (provs.size() == 1)
        return generateShapeLookup("shapes", provs.front(), ctx);

    std::stringstream stream;
    stream << "  let shapes = load_shape_table(device, @|type_id, data| { match type_id {" << std::endl;

    for (size_t i = 0; i < provs.size() - 1; ++i)
        stream << "    " << provs.at(i)->id() << " => " << provs.at(i)->generateShapeCode(ctx) << "," << std::endl;

    stream << "    _ => " << provs.back()->generateShapeCode(ctx) << std::endl
           << "  }});" << std::endl;

    return stream.str();
}

std::string ShaderUtils::generateShapeLookup(const std::string& varname, ShapeProvider* provider, const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << "  let " << varname << " = load_shape_table(device, @|_, data| { " << std::endl
           << provider->generateShapeCode(ctx) << std::endl
           << "  });" << std::endl;
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
        stream << "  let " << output_var << " : MaterialShader = @|ctx| make_emissive_material(mat_id, bsdf_" << bsdf_id << "(ctx), medium_interface,"
               << " @finite_lights.get(" << light_id << "));" << std::endl
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

std::string ShaderUtils::inlinePayloadInfo(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "PayloadInfo{ primary_count = "
           << ctx.CurrentTechniqueVariantInfo().PrimaryPayloadCount
           << ", secondary_count = " << ctx.CurrentTechniqueVariantInfo().SecondaryPayloadCount
           << " }";

    return stream.str();
}
} // namespace IG