#include "DiffuseBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
DiffuseBSDF::DiffuseBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "diffuse")
    , mBSDF(bsdf)
{
}

void DiffuseBSDF::serialize(const SerializationInput& input) const
{
    const std::string roughnessName = mBSDF->property("alpha").isValid() ? "alpha" : "roughness";

    input.Tree.beginClosure(name());
    input.Tree.addColor("reflectance", *mBSDF, Vector3f::Constant(0.8f));
    input.Tree.addNumber(roughnessName, *mBSDF, 0.0f, ShadingTree::NumberOptions::Zero());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_diffuse_bsdf(ctx.surf, "
                 << input.Tree.getInline(roughnessName) << ", "
                 << input.Tree.getInline("reflectance") << ");" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<DiffuseBSDF> sDiffuseBSDF("diffuse", "roughdiffuse" /* Deprecated */);

} // namespace IG