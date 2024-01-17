#include "MapBSDF.h"
#include "ErrorBSDF.h"
#include "Logger.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/LoaderContext.h"
#include "loader/ShadingTree.h"

namespace IG {
MapBSDF::MapBSDF(Type type, const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, type == Type::Normal ? "normalmap" : "bumpmap")
    , mBSDF(bsdf)
    , mType(type)
{
}

void MapBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    const std::string inner = mBSDF->property("bsdf").getString();
    if (mType == Type::Normal)
        input.Tree.addColor("map", *mBSDF, Vector3f::Constant(1.0f), ShadingTree::ColorOptions::Dynamic());
    else
        input.Tree.addTexture("map", *mBSDF); // Better use some node system with explicit gradients...

    input.Tree.addNumber("strength", *mBSDF, 1.0f);

    const std::string bsdf_id = input.Tree.currentClosureID();
    if (inner.empty()) {
        IG_LOG(L_ERROR) << "Bsdf '" << name() << "' has no inner bsdf given" << std::endl;
        input.Stream << ErrorBSDF::inlineError(bsdf_id) << std::endl;
    } else {
        input.Stream << input.Tree.context().BSDFs->generate(inner, input.Tree);

        const std::string inner_id = input.Tree.getClosureID(inner);
        if (mType == Type::Normal) {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_normalmap(ctx, @|surf2| "
                         << "bsdf_" << inner_id << "(ctx.{surf=surf2}), "
                         << input.Tree.getInline("map") << ","
                         << input.Tree.getInline("strength") << ");" << std::endl;
        } else {
            input.Stream << input.Tree.pullHeader()
                         << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| make_bumpmap(ctx, "
                         << "@|surf2| -> Bsdf {  bsdf_" << inner_id << "(ctx.{surf=surf2}) }, "
                         << "texture_dx(" << input.Tree.getInline("map") << ", ctx).r, "
                         << "texture_dy(" << input.Tree.getInline("map") << ", ctx).r, "
                         << input.Tree.getInline("strength") << ");" << std::endl;
        }
    }

    input.Tree.endClosure();
}

static std::shared_ptr<BSDF> bsdf_bumpmap(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MapBSDF>(MapBSDF::Type::Bump, name, obj);
}
static BSDFRawRegister sBumpMapBSDF(bsdf_bumpmap, "bumpmap");

static std::shared_ptr<BSDF> bsdf_normalmap(const std::string& name, const std::shared_ptr<SceneObject>& obj)
{
    return std::make_shared<MapBSDF>(MapBSDF::Type::Normal, name, obj);
}
static BSDFRawRegister sNormalMapBSDF(bsdf_normalmap, "normalmap");
} // namespace IG