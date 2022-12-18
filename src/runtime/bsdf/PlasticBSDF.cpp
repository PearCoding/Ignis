#include "PlasticBSDF.h"
#include "SceneObject.h"
#include "loader/ShadingTree.h"

namespace IG {
PlasticBSDF::PlasticBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "plastic")
    , mBSDF(bsdf)
{
}

void PlasticBSDF::serialize(const SerializationInput& input) const
{
    const float ext_ior_default = getDielectricIOR("vacuum").value();
    const float int_ior_default = getDielectricIOR("polypropylene").value();

    const auto ext_spec = getDielectricIOR(mBSDF->property("ext_ior_material").getString());
    const auto int_spec = getDielectricIOR(mBSDF->property("int_ior_material").getString());

    input.Tree.beginClosure(name());
    input.Tree.addColor("specular_reflectance", *mBSDF, Vector3f::Ones());
    input.Tree.addColor("diffuse_reflectance", *mBSDF, Vector3f::Constant(0.8f));
    input.Tree.addNumber("ext_ior", *mBSDF, ext_spec.value_or(ext_ior_default));
    input.Tree.addNumber("int_ior", *mBSDF, int_spec.value_or(int_ior_default));

    setupRoughness(mBSDF, input);

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_plastic_bsdf(ctx.surf, "
                 << input.Tree.getInline("ext_ior") << ", "
                 << input.Tree.getInline("int_ior") << ", "
                 << input.Tree.getInline("diffuse_reflectance") << ", "
                 << "make_conductor_bsdf(ctx.surf, "
                 << "color_builtins::black, color_builtins::white, "
                 << input.Tree.getInline("specular_reflectance") << ", "
                 << "md_" << bsdf_id << "(ctx)));" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG