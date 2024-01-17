#include "TransparentBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
TransparentBSDF::TransparentBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "transparent")
    , mBSDF(bsdf)
{
}

void TransparentBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("color", *mBSDF, Vector3f::Ones());
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_perfect_refraction_bsdf(ctx.surf, "
                 << input.Tree.getInline("color") << ");" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<TransparentBSDF> sTransparentBSDF("transparent");
} // namespace IG