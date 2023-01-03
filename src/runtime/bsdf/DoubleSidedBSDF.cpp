#include "DoubleSidedBSDF.h"
#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
DoubleSidedBSDF::DoubleSidedBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "doublesided")
    , mBSDF(bsdf)
{
}

void DoubleSidedBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string inner = mBSDF->property("bsdf").getString();

    const std::string bsdf_id = input.Tree.currentClosureID();
    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name() << "' has no inner bsdf given" << std::endl;
        input.Stream << ErrorBSDF::inlineError(bsdf_id) << std::endl;
    } else {
        input.Stream << input.Tree.context().BSDFs->generate(inner, input.Tree);

        const std::string inner_id = input.Tree.getClosureID(inner);
        input.Stream << input.Tree.pullHeader()
                     << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_doublesided_bsdf(ctx.surf, "
                     << "@|surf2| -> Bsdf {  bsdf_" << inner_id << "(ctx.{surf=surf2}) });" << std::endl;
    }

    input.Tree.endClosure();
}
} // namespace IG