#include "LoaderBSDF.h"
#include "Loader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "ShadingTree.h"

#include "klems/KlemsLoader.h"

#include <chrono>

namespace IG {

constexpr float AIR_IOR            = 1.000277f;
constexpr float GLASS_IOR          = 1.55f;
constexpr float RUBBER_IOR         = 1.49f;
constexpr float ETA_DEFAULT        = 0.63660f;
constexpr float ABSORPTION_DEFAULT = 2.7834f;

static void setup_microfacet(ShadingTree& tree, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    tree.addNumber("alpha_u", ctx, *bsdf, 0.1f, false);
    tree.addNumber("alpha_v", ctx, *bsdf, 0.1f, false);
    tree.addNumber("alpha", ctx, *bsdf, 0.1f);

    // Not exposed in the documentation, but used internally
    tree.addNumber("alpha_scale", ctx, *bsdf, 1.0f);
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
        stream << "  let md_" << ShaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf, "
               << inlineIt("alpha_u") << ", "
               << inlineIt("alpha_v") << ");" << std::endl;
    } else {
        stream << "  let md_" << ShaderUtils::escapeIdentifier(name) << " = @|surf : SurfaceElement| " << distribution << "(surf, "
               << inlineIt("alpha") << ", "
               << inlineIt("alpha") << ");" << std::endl;
    }
    return stream.str();
}

static void bsdf_diffuse(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("reflectance", ctx, *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_diffuse_bsdf(surf, "
           << tree.getInline("reflectance") << ");" << std::endl;
}

static void bsdf_orennayar(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addNumber("alpha", ctx, *bsdf, 0.0f);
    tree.addColor("reflectance", ctx, *bsdf, Vector3f::Constant(0.5f));

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_orennayar_bsdf(surf, "
           << tree.getInline("alpha") << ", "
           << tree.getInline("reflectance") << ");" << std::endl;
}

static void bsdf_dielectric(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addColor("specular_transmittance", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("ext_ior", ctx, *bsdf, AIR_IOR);
    tree.addNumber("int_ior", ctx, *bsdf, GLASS_IOR);
    bool thin = bsdf->property("thin").getBool(bsdf->pluginType() == "thindielectric");

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| "
           << (thin ? "make_thin_glass_bsdf" : "make_glass_bsdf") << "(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("specular_transmittance") << ");" << std::endl;
}

static void bsdf_mirror(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_mirror_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ");" << std::endl;
}

static void bsdf_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("eta", ctx, *bsdf, ETA_DEFAULT);
    tree.addNumber("k", ctx, *bsdf, ABSORPTION_DEFAULT);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_conductor_bsdf(surf, "
           << tree.getInline("eta") << ", "
           << tree.getInline("k") << ", "
           << tree.getInline("specular_reflectance") << ");" << std::endl;
}

static void bsdf_rough_conductor(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("eta", ctx, *bsdf, ETA_DEFAULT);
    tree.addNumber("k", ctx, *bsdf, ABSORPTION_DEFAULT);

    setup_microfacet(tree, bsdf, ctx);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, false)
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_rough_conductor_bsdf(surf, "
           << tree.getInline("eta") << ", "
           << tree.getInline("k") << ", "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;
}

static void bsdf_metallic_roughness(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("base_color", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("metallic", ctx, *bsdf, 0);

    // Not exposed in the documentation, but used internally
    tree.addColor("base_color_scale", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("metallic_scale", ctx, *bsdf, 0);

    setup_microfacet(tree, bsdf, ctx);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, true) // TODO: I do not like this. We should not square here and nowhere else....
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_metallic_roughness_bsdf(surf, "
           << "color_mul(" << tree.getInline("base_color_scale") << ", " << tree.getInline("base_color") << "), "
           << tree.getInline("metallic_scale") << " * " << tree.getInline("metallic") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf));" << std::endl;
}

static void bsdf_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", ctx, *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", ctx, *bsdf, AIR_IOR);
    tree.addNumber("int_ior", ctx, *bsdf, RUBBER_IOR);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_mirror_bsdf(surf, " << tree.getInline("specular_reflectance") << "));" << std::endl;
}

static void bsdf_rough_plastic(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addColor("diffuse_reflectance", ctx, *bsdf, Vector3f::Constant(0.5f));
    tree.addNumber("ext_ior", ctx, *bsdf, AIR_IOR);
    tree.addNumber("int_ior", ctx, *bsdf, RUBBER_IOR);

    setup_microfacet(tree, bsdf, ctx);
    stream << tree.pullHeader()
           << inline_microfacet(name, tree, bsdf, false)
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_plastic_bsdf(surf, "
           << tree.getInline("ext_ior") << ", "
           << tree.getInline("int_ior") << ", "
           << tree.getInline("diffuse_reflectance") << ", "
           << "make_rough_conductor_bsdf(surf, 0, 1, "
           << tree.getInline("specular_reflectance") << ", "
           << "md_" << ShaderUtils::escapeIdentifier(name) << "(surf)));" << std::endl;
}

static void bsdf_phong(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("specular_reflectance", ctx, *bsdf, Vector3f::Ones());
    tree.addNumber("exponent", ctx, *bsdf, 30);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_phong_bsdf(surf, "
           << tree.getInline("specular_reflectance") << ", "
           << tree.getInline("exponent") << ");" << std::endl;
}

static void bsdf_principled(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    ShadingTree tree;
    tree.addColor("base_color", ctx, *bsdf, Vector3f::Constant(0.8f));
    tree.addNumber("ior", ctx, *bsdf, GLASS_IOR);
    tree.addNumber("diffuse_transmission", ctx, *bsdf, 0);
    tree.addNumber("specular_transmission", ctx, *bsdf, 0);
    tree.addNumber("specular_tint", ctx, *bsdf, 0);
    tree.addNumber("roughness", ctx, *bsdf, 0.5f);
    tree.addNumber("anisotropic", ctx, *bsdf, 0);
    tree.addNumber("flatness", ctx, *bsdf, 0);
    tree.addNumber("metallic", ctx, *bsdf, 0);
    tree.addNumber("sheen", ctx, *bsdf, 0);
    tree.addNumber("sheen_tint", ctx, *bsdf, 0);
    tree.addNumber("clearcoat", ctx, *bsdf, 0);
    tree.addNumber("clearcoat_gloss", ctx, *bsdf, 0);

    bool is_thin = bsdf->property("thin").getBool(false);

    stream << tree.pullHeader()
           << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_principled_bsdf(surf, "
           << tree.getInline("base_color") << ", "
           << tree.getInline("ior") << ", "
           << tree.getInline("diffuse_transmission") << ", "
           << tree.getInline("specular_transmission") << ", "
           << tree.getInline("specular_tint") << ", "
           << tree.getInline("roughness") << ", "
           << tree.getInline("anisotropic") << ", "
           << tree.getInline("flatness") << ", "
           << tree.getInline("metallic") << ", "
           << tree.getInline("sheen") << ", "
           << tree.getInline("sheen_tint") << ", "
           << tree.getInline("clearcoat") << ", "
           << tree.getInline("clearcoat_gloss") << ", "
           << (is_thin ? "true" : "false") << ");" << std::endl;
}

static void bsdf_twosided(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    // Ignore
    const std::string other = bsdf->property("bsdf").getString();
    stream << LoaderBSDF::generate(other, ctx);
    stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = bsdf_" << ShaderUtils::escapeIdentifier(other) << ";" << std::endl;
}

static void bsdf_passthrough(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>&, const LoaderContext& ctx)
{
    IG_UNUSED(ctx);
    stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, surf| make_passthrough_bsdf(surf);" << std::endl;
}

static void bsdf_blend(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    const std::string first  = bsdf->property("first").getString();
    const std::string second = bsdf->property("second").getString();

    if (first.empty() || second.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdfs given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, _surf| make_error_bsdf();" << std::endl;
    } else if (first == second) {
        // Ignore it
        stream << LoaderBSDF::generate(first, ctx);
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " = bsdf_" << ShaderUtils::escapeIdentifier(first) << ";" << std::endl;
    } else {
        ShadingTree tree;
        tree.addNumber("weight", ctx, *bsdf, 0.5f);

        stream << LoaderBSDF::generate(first, ctx);
        stream << LoaderBSDF::generate(second, ctx);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf("
               << "bsdf_" << ShaderUtils::escapeIdentifier(first) << "(ray, hit, surf), "
               << "bsdf_" << ShaderUtils::escapeIdentifier(second) << "(ray, hit, surf), "
               << tree.getInline("weight") << ");" << std::endl;
    }
}

static void bsdf_mask(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    const std::string masked = bsdf->property("bsdf").getString();

    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, _surf| make_error_bsdf();" << std::endl;
    } else {
        ShadingTree tree;
        tree.addNumber("weight", ctx, *bsdf, 0.5f);

        stream << LoaderBSDF::generate(masked, ctx);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_mix_bsdf("
               << "bsdf_" << ShaderUtils::escapeIdentifier(masked) << "(ray, hit, surf), "
               << "make_passthrough_bsdf(surf), "
               << tree.getInline("weight") << ");" << std::endl;
    }
}

static void bsdf_normalmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    const std::string inner = bsdf->property("bsdf").getString();
    ShadingTree tree;
    tree.addColor("map", ctx, *bsdf, Vector3f::Constant(1.0f));

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, _surf| make_error_bsdf();" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, ctx);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_normalmap(surf, @|surf2| -> Bsdf { "
               << " bsdf_" << ShaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << tree.getInline("map") << ");" << std::endl;
    }
}

static void bsdf_bumpmap(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& bsdf, const LoaderContext& ctx)
{
    const std::string inner = bsdf->property("bsdf").getString();
    ShadingTree tree;
    tree.addTexture("map", ctx, *bsdf); // Better use some node system with explicit gradients...
    tree.addNumber("strength", ctx, *bsdf);

    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name << "' has no inner bsdf given" << std::endl;
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, _surf| make_error_bsdf();" << std::endl;
    } else {
        stream << LoaderBSDF::generate(inner, ctx);

        stream << tree.pullHeader()
               << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|ray, hit, surf| make_bumpmap(surf, "
               << "@|surf2| -> Bsdf {  bsdf_" << ShaderUtils::escapeIdentifier(inner) << "(ray, hit, surf2) }, "
               << "texture_dx(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << "texture_dy(" << tree.getInline("map") << ", surf.tex_coords).r, "
               << tree.getInline("strength") << ");" << std::endl;
    }
}

using BSDFLoader = void (*)(std::ostream& stream, const std::string& name, const std::shared_ptr<Parser::Object>& light, const LoaderContext& ctx);
static struct {
    const char* Name;
    BSDFLoader Loader;
} _generators[] = {
    { "diffuse", bsdf_diffuse },
    { "roughdiffuse", bsdf_orennayar },
    { "glass", bsdf_dielectric },
    { "dielectric", bsdf_dielectric },
    { "roughdielectric", bsdf_dielectric }, // TODO
    { "thindielectric", bsdf_dielectric },
    { "mirror", bsdf_mirror }, // Specialized conductor
    { "conductor", bsdf_conductor },
    { "roughconductor", bsdf_rough_conductor },
    { "metallic_roughness", bsdf_metallic_roughness },
    { "phong", bsdf_phong },
    { "disney", bsdf_principled },
    { "principled", bsdf_principled },
    { "plastic", bsdf_plastic },
    { "roughplastic", bsdf_rough_plastic },
    /*{ "klems", bsdf_klems },*/
    { "blend", bsdf_blend },
    { "mask", bsdf_mask },
    { "twosided", bsdf_twosided },
    { "passthrough", bsdf_passthrough },
    { "null", bsdf_passthrough },
    { "bumpmap", bsdf_bumpmap },
    { "normalmap", bsdf_normalmap },
    { "", nullptr }
};

std::string LoaderBSDF::generate(const std::string& name, const LoaderContext& ctx)
{
    std::stringstream stream;
    const auto bsdf = ctx.Scene.bsdf(name);

    bool error = false;

    if (!bsdf) {
        IG_LOG(L_ERROR) << "Unknown bsdf '" << name << "'" << std::endl;
        error = true;
    } else {

        bool found = false;
        for (size_t i = 0; _generators[i].Loader; ++i) {
            if (_generators[i].Name == bsdf->pluginType()) {
                _generators[i].Loader(stream, name, bsdf, ctx);
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
        stream << "  let bsdf_" << ShaderUtils::escapeIdentifier(name) << " : BSDFShader = @|_ray, _hit, _surf| make_error_bsdf();" << std::endl;
    }

    return stream.str();
}
} // namespace IG
