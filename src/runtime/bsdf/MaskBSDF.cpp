#include "MaskBSDF.h"
#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
MaskBSDF::MaskBSDF(Type type, const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, type == Type::Mask ? "mask" : "cutoff")
    , mBSDF(bsdf)
    , mType(type)
{
}

void MaskBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string masked = mBSDF->property("bsdf").getString();
    const bool inverted      = mBSDF->property("inverted").getBool();

    const std::string bsdf_id = input.Tree.currentClosureID();
    if (masked.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name() << "' has no inner bsdf given" << std::endl;
        input.Stream << ErrorBSDF::inlineError(bsdf_id) << std::endl;
    } else {
        input.Tree.addNumber("weight", *mBSDF, 0.5f);
        if (mType == Type::Cutoff)
            input.Tree.addNumber("cutoff", *mBSDF, 0.5f);

        const std::string masked_id = input.Tree.getClosureID(masked);

        input.Stream << input.Tree.context().BSDFs->generate(masked, input.Tree);

        input.Stream << input.Tree.pullHeader()
                     << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_mix_bsdf(";
        if (inverted) {
            input.Stream
                << "make_passthrough_bsdf(ctx.surf), "
                << "bsdf_" << masked_id << "(ctx), ";
        } else {
            input.Stream
                << "bsdf_" << masked_id << "(ctx), "
                << "make_passthrough_bsdf(ctx.surf), ";
        }

        if (mType == Type::Mask) {
            input.Stream << input.Tree.getInline("weight") << ");" << std::endl;
        } else {
            input.Stream << "select("
                         << input.Tree.getInline("weight") << " < "
                         << input.Tree.getInline("cutoff") << ", 0:f32, 1:f32));" << std::endl;
        }
    }

    input.Tree.endClosure();
}

static std::shared_ptr<BSDF> bsdf_mask(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MaskBSDF>(MaskBSDF::Type::Mask, name, obj);
}
static BSDFRawRegister sMaskBSDF(bsdf_mask, "mask");

static std::shared_ptr<BSDF> bsdf_cutoff(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MaskBSDF>(MaskBSDF::Type::Cutoff, name, obj);
}
static BSDFRawRegister sCutoffBSDF(bsdf_cutoff, "cutoff");

} // namespace IG