#include "PhongBSDF.h"
#include "SceneObject.h"
#include "loader/ShadingTree.h"

namespace IG {
PhongBSDF::PhongBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "phong")
    , mBSDF(bsdf)
{
}

void PhongBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("specular_reflectance", *mBSDF, Vector3f::Ones());
    input.Tree.addNumber("exponent", *mBSDF, 30, ShadingTree::NumberOptions::Dynamic());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_phong_bsdf(ctx.surf, "
                 << input.Tree.getInline("specular_reflectance") << ", "
                 << input.Tree.getInline("exponent") << ");" << std::endl;

    input.Tree.endClosure();
}
} // namespace IG