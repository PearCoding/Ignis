#include "BlendBSDF.h"
#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
BlendBSDF::BlendBSDF(Type type, const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, type == Type::Mix ? "blend" : "add")
    , mBSDF(bsdf)
    , mType(type)
{
}

void BlendBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string first  = mBSDF->property("first").getString();
    const std::string second = mBSDF->property("second").getString();

    const std::string bsdf_id = input.Tree.currentClosureID();
    if (first.empty() || second.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name() << "' has no inner bsdfs given" << std::endl;
        input.Stream << ErrorBSDF::inlineError(bsdf_id) << std::endl;
    } else if (first == second) {
        input.Stream << input.Tree.context().BSDFs->generate(first, input.Tree);
        if (mType == Type::Mix) {
            // Ignore it
            input.Stream << "  let bsdf_" << bsdf_id << " = bsdf_" << input.Tree.getClosureID(first) << ";" << std::endl;
        } else {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_add_bsdf("
                         << "bsdf_" << input.Tree.getClosureID(first) << "(ctx), "
                         << "bsdf_" << input.Tree.getClosureID(first) << "(ctx));" << std::endl;
        }
    } else {
        if (mType == Type::Mix)
            input.Tree.addNumber("weight", *mBSDF, 0.5f);

        input.Stream << input.Tree.context().BSDFs->generate(first, input.Tree);
        input.Stream << input.Tree.context().BSDFs->generate(second, input.Tree);

        if (mType == Type::Mix) {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_mix_bsdf("
                         << "bsdf_" << input.Tree.getClosureID(first) << "(ctx), "
                         << "bsdf_" << input.Tree.getClosureID(second) << "(ctx), "
                         << input.Tree.getInline("weight") << ");" << std::endl;
        } else {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_add_bsdf("
                         << "bsdf_" << input.Tree.getClosureID(first) << "(ctx), "
                         << "bsdf_" << input.Tree.getClosureID(second) << "(ctx));" << std::endl;
        }
    }
    input.Tree.endClosure();
}
} // namespace IG