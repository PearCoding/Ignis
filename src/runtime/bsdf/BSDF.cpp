#include "BSDF.h"
#include "SceneObject.h"
#include "StringUtils.h"
#include "loader/ShadingTree.h"

namespace IG {
const static std::unordered_map<std::string_view, float> Dielectrics = {
    { "vacuum", 1.0f },
    { "bk7", 1.5046f },
    { "glass", 1.5046f },
    { "helium", 1.00004f },
    { "hydrogen", 1.00013f },
    { "air", 1.000277f },
    { "water", 1.333f },
    { "ethanol", 1.361f },
    { "diamond", 2.419f },
    { "polypropylene", 1.49f }
};

std::optional<float> BSDF::getDielectricIOR(const std::string& mat)
{
    const std::string material = to_lowercase(mat);
    if (auto it = Dielectrics.find(material); it != Dielectrics.end())
        return it->second;
    else
        return std::nullopt;
}

// Materials from https://chris.hindefjord.se/resources/rgb-ior-metals/
const static std::unordered_map<std::string_view, BSDF::ConductorSpec> Conductors = {
    { "aluminum", { { 1.34560f, 0.96521f, 0.61722f }, { 7.47460f, 6.39950f, 5.30310f } } },
    { "brass", { { 0.44400f, 0.52700f, 1.09400f }, { 3.69500f, 2.76500f, 1.82900f } } },
    { "copper", { { 0.27105f, 0.67693f, 1.31640f }, { 3.60920f, 2.62480f, 2.29210f } } },
    { "gold", { { 0.18299f, 0.42108f, 1.37340f }, { 3.4242f, 2.34590f, 1.77040f } } },
    { "iron", { { 2.91140f, 2.94970f, 2.58450f }, { 3.08930f, 2.93180f, 2.76700f } } },
    { "lead", { { 1.91000f, 1.83000f, 1.44000f }, { 3.51000f, 3.40000f, 3.18000f } } },
    { "mercury", { { 2.07330f, 1.55230f, 1.06060f }, { 5.33830f, 4.65100f, 3.86280f } } },
    { "platinum", { { 2.37570f, 2.08470f, 1.84530f }, { 4.26550f, 3.71530f, 3.13650f } } },
    { "silver", { { 0.15943f, 0.14512f, 0.13547f }, { 3.92910f, 3.19000f, 2.38080f } } },
    { "titanium", { { 2.74070f, 2.54180f, 2.26700f }, { 3.81430f, 3.43450f, 3.03850f } } },
    { "none", { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } } }
};

std::optional<BSDF::ConductorSpec> BSDF::getConductor(const std::string& mat)
{
    const std::string material = to_lowercase(mat);
    if (auto it = Conductors.find(material); it != Conductors.end())
        return it->second;
    else
        return std::nullopt;
}

bool BSDF::setupRoughness(const std::shared_ptr<SceneObject>& bsdf, const SerializationInput& input)
{
    const bool useOldName   = bsdf->property("alpha").isValid() || bsdf->property("alpha_u").isValid() || bsdf->property("alpha_v").isValid();
    const std::string param = useOldName ? "alpha" : "roughness";

    // Check if simply delta
    if (!bsdf->hasProperty(param) && !bsdf->hasProperty(param + "_u") && !bsdf->hasProperty(param + "_v")) {
        const std::string md_id = input.Tree.currentClosureID();
        input.Stream << "  let md_" << md_id << " = @|ctx : ShadingContext| microfacet::make_delta_distribution(ctx.surf.local);" << std::endl;
        return false;
    }

    const bool isExplicit = input.Tree.hasParameter(param + "_u") || input.Tree.hasParameter(param + "_v");

    if (isExplicit) {
        input.Tree.addNumber(param + "_u", *bsdf, 0.1f, ShadingTree::NumberOptions::Zero());
        input.Tree.addNumber(param + "_v", *bsdf, 0.1f, ShadingTree::NumberOptions::Zero());
    } else {
        input.Tree.addNumber(param, *bsdf, 0.1f, ShadingTree::NumberOptions::Zero());
        input.Tree.addNumber("anisotropic", *bsdf, 0.0f, ShadingTree::NumberOptions::Zero());
    }

    std::string distribution = "microfacet::make_vndf_ggx_distribution(ctx.surf.face_normal, ";
    if (bsdf->property("distribution").type() == SceneProperty::PT_STRING) {
        std::string type = bsdf->property("distribution").getString();
        if (type == "ggx") {
            distribution = "microfacet::make_ggx_distribution(";
        } else if (type == "beckmann") {
            distribution = "microfacet::make_beckmann_distribution(";
        }
    }

    const std::string md_id = input.Tree.currentClosureID();
    if (isExplicit) {
        input.Stream << input.Tree.pullHeader()
                     << "  let md_" << md_id << " = @|ctx : ShadingContext| " << distribution << "ctx.surf.local, "
                     << input.Tree.getInline(param + "_u") << ", "
                     << input.Tree.getInline(param + "_v") << ");" << std::endl;
    } else {
        input.Stream << input.Tree.pullHeader()
                     << "  let md_" << md_id << " = @|ctx : ShadingContext| { let (ru, rv) = microfacet::compute_explicit("
                     << input.Tree.getInline(param) << ", "
                     << input.Tree.getInline("anisotropic") << ");"
                     << distribution << "ctx.surf.local, ru, rv) };" << std::endl;
    }
    return true;
}

} // namespace IG