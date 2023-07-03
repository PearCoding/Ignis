#include "RadRoosBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
RadRoosBSDF::RadRoosBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "rad_roos")
    , mBSDF(bsdf)
{
}

void RadRoosBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addNumber("refl_w", *mBSDF, 0.0f);
    input.Tree.addNumber("refl_p", *mBSDF, 0.0f);
    input.Tree.addNumber("refl_q", *mBSDF, 0.0f);
    input.Tree.addNumber("trns_w", *mBSDF, 0.0f);
    input.Tree.addNumber("trns_p", *mBSDF, 0.0f);
    input.Tree.addNumber("trns_q", *mBSDF, 0.0f);
    input.Tree.addColor("reflection_front_diffuse", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("reflection_back_diffuse", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("transmission_diffuse", *mBSDF, Vector3f::Zero());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_rad_roos_bsdf(ctx.surf, -vec3_dot(ctx.ray.dir, ctx.surf.local.col(2)),"
                 << input.Tree.getInline("refl_w") << ", "
                 << input.Tree.getInline("refl_p") << ", "
                 << input.Tree.getInline("refl_q") << ", "
                 << input.Tree.getInline("trns_w") << ", "
                 << input.Tree.getInline("trns_p") << ", "
                 << input.Tree.getInline("trns_q") << ", "
                 << input.Tree.getInline("reflection_front_diffuse") << ", "
                 << input.Tree.getInline("reflection_back_diffuse") << ", "
                 << input.Tree.getInline("transmission_diffuse") << ");" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<RadRoosBSDF> sRadRoosBSDF("rad_roos");
} // namespace IG