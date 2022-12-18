#include "ConductorBSDF.h"
#include "SceneObject.h"
#include "loader/ShadingTree.h"

namespace IG {
ConductorBSDF::ConductorBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "conductor")
    , mBSDF(bsdf)
{
}

void ConductorBSDF::serialize(const SerializationInput& input) const
{
    const auto def  = getConductor("none").value();
    const auto spec = getConductor(mBSDF->property("material").getString());

    input.Tree.beginClosure(name());
    input.Tree.addColor("specular_reflectance", *mBSDF, Vector3f::Ones());
    input.Tree.addColor("eta", *mBSDF, spec.value_or(def).Eta);
    input.Tree.addColor("k", *mBSDF, spec.value_or(def).Kappa);

    setupRoughness(mBSDF, input);

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_conductor_bsdf(ctx.surf, "
                 << input.Tree.getInline("eta") << ", "
                 << input.Tree.getInline("k") << ", "
                 << input.Tree.getInline("specular_reflectance") << ", "
                 << "md_" << bsdf_id << "(ctx));" << std::endl;
    input.Tree.endClosure();
}
} // namespace IG