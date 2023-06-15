#include "IgnoreBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
IgnoreBSDF::IgnoreBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "ignore")
    , mBSDF(bsdf)
{
}

void IgnoreBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string other   = mBSDF->property("bsdf").getString();
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.context().BSDFs->generate(other, input.Tree)
                 << "  let bsdf_" << bsdf_id << " = bsdf_" << input.Tree.getClosureID(other) << ";" << std::endl;
    input.Tree.endClosure();
}

//static BSDFRegister<IgnoreBSDF> sIgnoreBSDF();
} // namespace IG