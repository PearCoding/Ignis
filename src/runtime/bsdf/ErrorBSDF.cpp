#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/ShadingTree.h"

namespace IG {
ErrorBSDF::ErrorBSDF(const std::string& name)
    : BSDF(name, "error")
{
}

void ErrorBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << inlineError(bsdf_id) << std::endl;
    input.Tree.endClosure();
}

std::string ErrorBSDF::inlineError(const std::string& name)
{
    return "  let bsdf_" + name + " : BSDFShader = @|ctx| make_error_bsdf(ctx.surf);";
}
} // namespace IG