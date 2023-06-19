#include "DielectricBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
DielectricBSDF::DielectricBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "dielectric")
    , mBSDF(bsdf)
{
}

void DielectricBSDF::serialize(const SerializationInput& input) const
{
    const float ext_ior_default = getDielectricIOR("vacuum").value();
    const float int_ior_default = getDielectricIOR("bk7").value();

    const auto ext_spec = getDielectricIOR(mBSDF->property("ext_ior_material").getString());
    const auto int_spec = getDielectricIOR(mBSDF->property("int_ior_material").getString());

    input.Tree.beginClosure(name());
    input.Tree.addColor("specular_reflectance", *mBSDF, Vector3f::Ones());
    input.Tree.addColor("specular_transmittance", *mBSDF, Vector3f::Ones());
    input.Tree.addNumber("ext_ior", *mBSDF, ext_spec.value_or(ext_ior_default));
    input.Tree.addNumber("int_ior", *mBSDF, int_spec.value_or(int_ior_default));

    const bool thin = mBSDF->property("thin").getBool(false);
    setupRoughness(mBSDF, input);

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_dielectric_bsdf(ctx.surf, "
                 << input.Tree.getInline("ext_ior") << ", "
                 << input.Tree.getInline("int_ior") << ", "
                 << input.Tree.getInline("specular_reflectance") << ", "
                 << input.Tree.getInline("specular_transmittance") << ", "
                 << "md_" << bsdf_id << "(ctx), "
                 << (thin ? "true" : "false") << ");" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<DielectricBSDF> sDielectricBSDF("glass" /* Deprecated */, "dielectric", "roughdielectric" /* Deprecated */, "thindielectric" /* Deprecated */);

} // namespace IG