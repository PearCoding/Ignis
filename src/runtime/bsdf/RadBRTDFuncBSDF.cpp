#include "RadBRTDFuncBSDF.h"
#include "SceneObject.h"
#include "loader/LoaderBSDF.h"
#include "loader/ShadingTree.h"

namespace IG {
RadBRTDFuncBSDF::RadBRTDFuncBSDF(const std::string& name, const std::shared_ptr<SceneObject>& bsdf)
    : BSDF(name, "rad_brtdfunc")
    , mBSDF(bsdf)
{
}

void RadBRTDFuncBSDF::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());
    input.Tree.addColor("reflection_specular", *mBSDF, Vector3f::Ones());
    input.Tree.addColor("transmission_specular", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("directional_diffuse", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("reflection_front_diffuse", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("reflection_back_diffuse", *mBSDF, Vector3f::Zero());
    input.Tree.addColor("transmission_diffuse", *mBSDF, Vector3f::Zero());

    const std::string bsdf_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader()
                 << "  let bsdf_" << bsdf_id << " : BSDFShader = @|ctx| "
                 << "make_rad_brtdfunc_bsdf(ctx.surf, "
                 << input.Tree.getInline("reflection_specular") << ", "
                 << input.Tree.getInline("transmission_specular") << ", "
                 << input.Tree.getInline("directional_diffuse") << ", "
                 << input.Tree.getInline("reflection_front_diffuse") << ", "
                 << input.Tree.getInline("reflection_back_diffuse") << ", "
                 << input.Tree.getInline("transmission_diffuse") << ");" << std::endl;
    input.Tree.endClosure();
}

static BSDFRegister<RadBRTDFuncBSDF> sRadBRTDBSDF("rad_brtdfunc");
} // namespace IG