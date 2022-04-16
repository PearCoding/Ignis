#include "LoaderBSDF.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"

#include "klems/KlemsLoader.h"
#include "klems/TensorTreeLoader.h"

#include <chrono>

namespace IG {

constexpr float AIR_IOR            = 1.000277f;
constexpr float GLASS_IOR          = 1.55f;
constexpr float RUBBER_IOR         = 1.49f;
constexpr float ETA_DEFAULT        = 0.63660f;
constexpr float ABSORPTION_DEFAULT = 2.7834f;

static void setup_microfacet(const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.addNumber("alpha_u", *bsdf, 0.1f, false);
    tree.addNumber("alpha_v", *bsdf, 0.1f, false);
    tree.addNumber("alpha", *bsdf, 0.1f);

    // Not exposed in the documentation, but used internally
    tree.addNumber("alpha_scale", *bsdf, 1.0f);
}

static std::string inline_microfacet(const std::string& name, const ShadingTree& tree, const std::shared_ptr<Parser::Object>& bsdf, bool square)
{
    const auto inlineIt = [&](const std::string& prop) {
        std::stringstream stream2;
        stream2 << tree.getInline("alpha_scale") << " * (" << tree.getInline(prop) << ")";
        if (square)
            stream2 << " * (" << tree.getInline(prop) << ")";
        return stream2.str();
    };

    std::string distribution = "make_vndf_ggx_distribution";
    if (bsdf->property("distribution").type() == Parser::PT_STRING) {
        std::string type = bsdf->property("distribution").getString();
        if (type == "ggx") {
            distribution = "make_ggx_distribution";
        } else if (type == "beckmann") {
            distribution = "make_beckmann_distribution";
        }
    }

    std::stringstream stream;
    if (tree.hasParameter("alpha_u")) {
        stream << "  let md_" << ShaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf.local, "
               << inlineIt("alpha_u") << ", "
               << inlineIt("alpha_v") << ");" << std::endl;
    } else {
        stream << "  let md_" << ShaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf.local, "
               << inlineIt("alpha") << ", "
               << inlineIt("alpha") << ");" << std::endl;
    }
    return stream.str();
}

static void bsdf_diffuse(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("reflectance", *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_diffuse_bsdf(surf, "
           << tree.getInline("reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_orennayar(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addNumber("alpha", *bsdf, 0.0f);
    tree.addColor("reflectance", *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_orennayar_bsdf(surf, "
           << tree.getInline("alpha") << ", "
           << tree.getInline("reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("specular_transmittance", *bsdf, Vector3f::Ones());
    tree.addNumber("ext_ior", *bsdf, AIR_IOR);
    tree.addNumber("int_ior", *bsdf, GLASS_IOR);
    bool thin = bsdf->property("thin").getBool(bsdf->pluginType() == "thindielectric");

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| "
           << (thin ? "make_thin_glass_bsdf" : "make_glass_bsdf") << "(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("specular_transmittance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("specular_transmittance", *bsdf, Vector3f::Ones());
    tree.addNumber("ext_ior", *bsdf, AIR_IOR);
    tree.addNumber("int_ior", *bsdf, GLASS_IOR);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, false)
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_rough_glass_bsdf(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("specular_transmittance") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;

    tree.endClosure();
}

static void bsdf_mirror(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_mirror_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addNumber("eta", *bsdf, ETA_DEFAULT);
    tree.addNumber("k", *bsdf, ABSORPTION_DEFAULT);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_conductor_bsdf(surf, "
           << tree.getInline("eta") << ", "
           << tree.getInline("k") << ", "
           << tree.getInline("specular_reflectance") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addNumber("eta", *bsdf, ETA_DEFAULT);
    tree.addNumber("k", *bsdf, ABSORPTION_DEFAULT);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, false)
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_rough_conductor_bsdf(surf, "
           << tree.getInline("eta") << ", "
           << tree.getInline("k") << ", "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;

    tree.endClosure();
}

static void bsdf_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", *bsdf, AIR_IOR);
    tree.addNumber("int_ior", *bsdf, RUBBER_IOR);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_mirror_bsdf(surf, " << tree.getInline("specular_reflectance") << "));" << std::endl;

    tree.endClosure();
}

static void bsdf_rough_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", *bsdf, AIR_IOR);
    tree.addNumber("int_ior", *bsdf, RUBBER_IOR);

    setup_microfacet(bsdf, tree);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, false)
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_rough_conductor_bsdf(surf, 0, 1, "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf)));" << std::endl;

    tree.endClosure();
}

static void bsdf_phong(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("specular_reflectance", *bsdf, Vector3f::Ones());
    tree.addNumber("exponent", *bsdf, 30);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_phong_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("exponent") << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_principled(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Constant(0.8f));
    tree.addNumber("ior", *bsdf, GLASS_IOR);
    tree.addNumber("diffuse_transmission", *bsdf, 0);
    tree.addNumber("specular_transmission", *bsdf, 0);
    tree.addNumber("specular_tint", *bsdf, 0);
    tree.addNumber("roughness", *bsdf, 0.5f);
    tree.addNumber("anisotropic", *bsdf, 0);
    tree.addNumber("flatness", *bsdf, 0);
    tree.addNumber("metallic", *bsdf, 0);
    tree.addNumber("sheen", *bsdf, 0);
    tree.addNumber("sheen_tint", *bsdf, 0);
    tree.addNumber("clearcoat", *bsdf, 0);
    tree.addNumber("clearcoat_gloss", *bsdf, 0);
    tree.addNumber("clearcoat_roughness", *bsdf, 0.1f);

    bool is_thin = bsdf->property("thin").getBool(false);

    // Not exposed in the documentation, but used internally until we have proper shading nodes
    tree.addColor("base_color_scale", *bsdf, Vector3f::Ones());
    tree.addNumber("metallic_scale", *bsdf, 1);
    tree.addNumber("roughness_scale", *bsdf, 1);
    tree.addNumber("specular_transmission_scale", *bsdf, 1);
    tree.addNumber("diffuse_transmission_scale", *bsdf, 1);
    tree.addNumber("clearcoat_scale", *bsdf, 1);
    tree.addNumber("clearcoat_roughness_scale", *bsdf, 1);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_principled_bsdf(surf, "
           << "color_mul(" << tree.getInline("base_color_scale") << ", " << tree.getInline("base_color") << "), "
           << tree.getInline("ior") << ", "
           << tree.getInline("diffuse_transmission_scale") << " * " << tree.getInline("diffuse_transmission") << ", "
           << tree.getInline("specular_transmission_scale") << " * " << tree.getInline("specular_transmission") << ", "
           << tree.getInline("specular_tint") << ", "
           << tree.getInline("roughness_scale") << " * " << tree.getInline("roughness") << ", "
           << tree.getInline("anisotropic") << ", "
           << tree.getInline("flatness") << ", "
           << tree.getInline("metallic_scale") << " * " << tree.getInline("metallic") << ", "
           << tree.getInline("sheen") << ", "
           << tree.getInline("sheen_tint") << ", "
           << tree.getInline("clearcoat_scale") << " * " << tree.getInline("clearcoat") << ", "
           << tree.getInline("clearcoat_gloss") << ", "
           << tree.getInline("clearcoat_roughness_scale") << " * " << tree.getInline("clearcoat_roughness") << ", "
           << (is_thin ? "true" : "false") << ");" << std::endl;

    tree.endClosure();
}

static std::string setup_klems(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/klems_" + ShaderUtils::escapeIdentifier(name) + ".bin";

    KlemsLoader::prepare(filename, path);
    return path;
}

static void bsdf_klems(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Ones());

    const std::string id         = ShaderUtils::escapeIdentifier(name);
    const std::string klems_path = setup_klems(name, bsdf, tree.context());

    stream << tree.pullHeader()
           << "  let klems_" << id << " = make_klems_model(device.load_buffer(\"" << klems_path << "\"), "
           << "device.load_host_buffer(\"" << klems_path << "\"));" << std::endl
           << "  let bsdf_" << id << " : BSDFShader = @|_ray, _hit, surf| make_klems_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << "klems_" << id << ");" << std::endl;

    tree.endClosure();
}

static std::pair<std::string, TensorTreeSpecification> setup_tensortree(const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    auto filename = ctx.handlePath(bsdf->property("filename").getString(), *bsdf);

    std::filesystem::create_directories("data/"); // Make sure this directory exists
    std::string path = "data/tt_" + ShaderUtils::escapeIdentifier(name) + ".bin";

    TensorTreeSpecification spec{};
    TensorTreeLoader::prepare(filename, path, spec);
    return { path, spec };
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& parent, const TensorTreeComponentSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeComponentSpecification{ ndim=" << parent.ndim
           << ", node_count=" << spec.node_count
           << ", value_count=" << spec.value_count
           << "}";
    return stream.str();
}

static inline std::string dump_tt_specification(const TensorTreeSpecification& spec)
{
    std::stringstream stream;
    stream << "TensorTreeSpecification{ ndim=" << spec.ndim
           << ", has_reflection=" << (spec.has_reflection ? "true" : "false")
           << ", front_reflection=" << dump_tt_specification(spec, spec.front_reflection)
           << ", back_reflection=" << dump_tt_specification(spec, spec.back_reflection)
           << ", front_transmission=" << dump_tt_specification(spec, spec.front_transmission)
           << ", back_transmission=" << dump_tt_specification(spec, spec.back_transmission)
           << "}";
    return stream.str();
}

static void bsdf_tensortree(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    tree.beginClosure();
    tree.addColor("base_color", *bsdf, Vector3f::Ones());

    const std::string id = ShaderUtils::escapeIdentifier(name);
    const auto data      = setup_tensortree(name, bsdf, tree.context());

    const std::string buffer_path      = std::get<0>(data);
    const TensorTreeSpecification spec = std::get<1>(data);

    stream << tree.pullHeader()
           << "  let tt_" << id << " = make_tensortree_model(device.request_debug_output(), device.load_buffer(\"" << buffer_path << "\"), "
           << dump_tt_specification(spec) << ");" << std::endl
           << "  let bsdf_" << id << " : BSDFShader = @|_ray, _hit, surf| make_tensortree_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << "tt_" << id << ");" << std::endl;

    tree.endClosure();
}

static void bsdf_twosided(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    // Ignore
    const std::string other = bsdf->property("bsdf").getString();
    stream << LoaderBSDF::generate(other, tree);
    stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = bsdf_" << ShaderUtils::escapeIdentifier(other) << ";" << std::endl;
}

static void bsdf_passthrough(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>&, ShadingTree& tree)
{
    IG_UNUSED(tree);
    stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_passthrough_bsdf(surf);" << std::endl;
}

static void bsdf_blend(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string first  = bsdf->property("first").getString();
    const std::string second = bsdf->property("second").getString();

    if (first.empty() || second.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdfs given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else if (first == second) {
        // Ignore it
        stream << LoaderBSDF::generate(first, tree);
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = bsdf_" << ShaderUtils::escapeIdentifier(first) << ";" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(first, tree);
        stream << LoaderBSDF::generate(second, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf("
               << "bsdf_" << ShaderUtils::escapeIdentifier(first) << "(ray, hit, surf), "
               << "bsdf_" << ShaderUtils::escapeIdentifier(second) << "(ray, hit, surf), "
               << tree.getInline("weight") << ");" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_mask(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string masked = bsdf->property("bsdf").getString();
    const bool inverted      = bsdf->property("inverted").getBool();

    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(masked, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf(";
        if (inverted) {
            stream
                << "make_passthrough_bsdf(surf), "
                << "bsdf_" << ShaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), ";
        } else {
            stream
                << "bsdf_" << ShaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), "
                << "make_passthrough_bsdf(surf), ";
        }
        stream << tree.getInline("weight") << ");" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_cutoff(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string masked = bsdf->property("bsdf").getString();
    const bool inverted      = bsdf->property("inverted").getBool();

    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        tree.beginClosure();
        tree.addNumber("weight", *bsdf, 0.5f); // Only useful with textures or other patterns
        tree.addNumber("cutoff", *bsdf, 0.5f);

        stream << LoaderBSDF::generate(masked, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf(";

        if (inverted) {
            stream
                << "make_passthrough_bsdf(surf), "
                << "bsdf_" << ShaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), ";
        } else {
            stream
                << "bsdf_" << ShaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), "
                << "make_passthrough_bsdf(surf), ";
        }

        stream << "if " << tree.getInline("weight") << " < " << tree.getInline("cutoff") << " { 0:f32 } else { 1:f32 } );" << std::endl;

        tree.endClosure();
    }
}

static void bsdf_normalmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string inner = bsdf->property("bsdf").getString();
    tree.beginClosure();
    tree.addColor("map", *bsdf, Vector3f::Constant(1.0f));
    tree.addNumber("strength", *bsdf, 1.0f);

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_normalmap(surf, @|surf2| -> Bsdf { "
               << " bsdf_" << ShaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << tree.getInline("map") << ","
               << tree.getInline("strength") << ");" << std::endl;
    }
    tree.endClosure();
}

static void bsdf_bumpmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, ShadingTree& tree)
{
    const std::string inner = bsdf->property("bsdf").getString();
    tree.beginClosure();
    tree.addTexture("map", *bsdf); // Better use some node system with explicit gradients...
    tree.addNumber("strength", *bsdf, 1.0f);

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, tree);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_bumpmap(surf, "
               << "@|surf2| -> Bsdf {  bsdf_" << ShaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << "texture_dx(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << "texture_dy(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << tree.getInline("strength") << ");" << std::endl;
    }
    tree.endClosure();
}

using BSDFLoader = void (*)(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, ShadingTree& tree);
static const struct {
    const char* Name;
    BSDFLoader Loader;
} _generators[] = {
    { "diffuse", bsdf_diffuse },
    { "roughdiffuse", bsdf_orennayar },
    { "glass", bsdf_dielectric },
    { "dielectric", bsdf_dielectric },
    { "roughdielectric", bsdf_rough_dielectric },
    { "thindielectric", bsdf_dielectric },
    { "mirror", bsdf_mirror }, // Specialized conductor
    { "conductor", bsdf_conductor },
    { "roughconductor", bsdf_rough_conductor },
    { "phong", bsdf_phong },
    { "disney", bsdf_principled },
    { "principled", bsdf_principled },
    { "plastic", bsdf_plastic },
    { "roughplastic", bsdf_rough_plastic },
    { "klems", bsdf_klems },
    { "tensortree", bsdf_tensortree },
    { "blend", bsdf_blend },
    { "mask", bsdf_mask },
    { "cutoff", bsdf_cutoff },
    { "twosided", bsdf_twosided },
    { "passthrough", bsdf_passthrough },
    { "null", bsdf_passthrough },
    { "bumpmap", bsdf_bumpmap },
    { "normalmap", bsdf_normalmap },
    { "", nullptr }
};

std::string LoaderBSDF::generate(const std::string& name, ShadingTree& tree)
{
    std::stringstream stream;
    const auto bsdf = tree.context().Scene.bsdf(name);

    bool error = false;

    if (!bsdf) {
        IG_LOG(L_ERROR) << "Unknown bsdf '" << name << "'" << std::endl;
        error = true;
    } else {

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == bsdf->pluginType()) {
                _generators[i].Loader(stream, name, bsdf, tree);
                found = true;
                break;
            }
        }

        if (!found) {
            IG_LOG(L_ERROR) << "No bsdf type '" << bsdf->pluginType() << "' available" << std::endl;
            error = true;
        }
    }

    if (error) {
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_error_bsdf(surf);" << std::endl;
    }

    return stream.str();
}
} // namespace IG
