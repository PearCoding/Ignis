#include "TransformBSDF.h"
#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
TransformBSDF::TransformBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "transform")
    , mBSDF(bsdf)
{
}

void TransformBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string inner = mBSDF->property("bsdf").getString();
    input.Tree.addVector("normal", *mBSDF, Vector3f::UnitZ());

    const std::string bsdf_id = input.Tree.currentClosureID();
    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name() << "' has no inner bsdf given" << std::endl;
        input.Stream << ErrorBSDF::inlineError(bsdf_id) << std::endl;
    } else {
        input.Stream << input.Tree.context().BSDFs->generate(inner, input.Tree);

        const std::string inner_id = input.Tree.getClosureID(inner);
        if (mBSDF->hasProperty("tangent")) {
            input.Tree.addVector("tangent", *mBSDF, Vector3f::UnitX());
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_normal_tangent_set(ctx, "
                         << "@|surf2| -> Bsdf {  bsdf_" << inner_id << "(ctx.{surf=surf2}) }, "
                         << input.Tree.getInline("normal")
                         << ", " << input.Tree.getInline("tangent") << ");" << std::endl;
        } else {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_normal_set(ctx, "
                         << "@|surf2| -> Bsdf {  bsdf_" << inner_id << "(ctx.{surf=surf2}) }, "
                         << input.Tree.getInline("normal") << ");" << std::endl;
        }
    }

    input.Tree.endClosure();
}
} // namespace IG