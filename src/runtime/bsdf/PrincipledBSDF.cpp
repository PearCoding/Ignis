#include "PrincipledBSDF.h"
#include "SceneObject.h"
#include "loader/ShadingTree.h"

namespace IG {
PrincipledBSDF::PrincipledBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "principled")
    , mBSDF(bsdf)
{
    mUseExplicitRoughness = bsdf->hasProperty("roughness_u") || bsdf->hasProperty("roughness_v");
}

void PrincipledBSDF::serialize(const SerializationInput& input) const
{
    const float int_ior_default = getDielectricIOR("bk7").value();
    const auto ior_spec         = getDielectricIOR(mBSDF->property("ior_material").getString());

    input.Tree.beginClosure(name());
    input.Tree.addColor("base_color", *mBSDF, Vector3f::Constant(0.8f));
    input.Tree.addNumber("ior", *mBSDF, ior_spec.value_or(int_ior_default), ShadingTree::NumberOptions::Dynamic());
    input.Tree.addNumber("diffuse_transmission", *mBSDF, 0.0f, ShadingTree::NumberOptions::Zero());
    input.Tree.addNumber("specular_transmission", *mBSDF, 0.0f);
    input.Tree.addNumber("specular_tint", *mBSDF, 0.0f);
    if (mUseExplicitRoughness) {
        input.Tree.addNumber("roughness_u", *mBSDF, 0.5f, ShadingTree::NumberOptions::Dynamic());
        input.Tree.addNumber("roughness_v", *mBSDF, 0.5f, ShadingTree::NumberOptions::Dynamic());
    } else {
        input.Tree.addNumber("roughness", *mBSDF, 0.5f, ShadingTree::NumberOptions::Dynamic());
        input.Tree.addNumber("anisotropic", *mBSDF, 0.0f, ShadingTree::NumberOptions::Zero());
    }
    input.Tree.addNumber("flatness", *mBSDF, 0.0f);
    input.Tree.addNumber("metallic", *mBSDF, 0.0f);
    input.Tree.addNumber("sheen", *mBSDF, 0.0f, ShadingTree::NumberOptions::Zero());
    input.Tree.addNumber("sheen_tint", *mBSDF, 0.0f);
    input.Tree.addNumber("clearcoat", *mBSDF, 0.0f, ShadingTree::NumberOptions::Zero());
    input.Tree.addNumber("clearcoat_gloss", *mBSDF, 0.0f);
    input.Tree.addNumber("clearcoat_roughness", *mBSDF, 0.1f, ShadingTree::NumberOptions::Dynamic());

    bool is_thin            = mBSDF->property("thin").getBool(false);
    bool clearcoat_top_only = mBSDF->property("clearcoat_top_only").getBool(true);

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| { ";

    if (mUseExplicitRoughness) {
        input.Stream << "let ru = " << input.Tree.getInline("roughness_u") << ";"
                     << "let rv = " << input.Tree.getInline("roughness_v") << ";";
    } else {
        input.Stream << "let (ru, rv) = principled::compute_roughness(" << input.Tree.getInline("roughness") << "," << input.Tree.getInline("anisotropic") << ");";
    }

    input.Stream << "make_principled_bsdf(ctx.surf, "
                 << input.Tree.getInline("base_color") << ", "
                 << input.Tree.getInline("ior") << ", "
                 << input.Tree.getInline("diffuse_transmission") << ", "
                 << input.Tree.getInline("specular_transmission") << ", "
                 << input.Tree.getInline("specular_tint") << ", "
                 << "ru, rv, "
                 << input.Tree.getInline("flatness") << ", "
                 << input.Tree.getInline("metallic") << ", "
                 << input.Tree.getInline("sheen") << ", "
                 << input.Tree.getInline("sheen_tint") << ", "
                 << input.Tree.getInline("clearcoat") << ", "
                 << input.Tree.getInline("clearcoat_gloss") << ", "
                 << input.Tree.getInline("clearcoat_roughness") << ", "
                 << (is_thin ? "true" : "false") << ", "
                 << (clearcoat_top_only ? "true" : "false") << ") };" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG