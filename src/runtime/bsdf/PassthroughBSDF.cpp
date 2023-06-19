#include "PassthroughBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
PassthroughBSDF::PassthroughBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "passthrough")
    , mBSDF(bsdf)
{
}

void PassthroughBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_passthrough_bsdf(ctx.surf);" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<PassthroughBSDF> sPassthroughBSDF("passthrough", "null");

} // namespace IG