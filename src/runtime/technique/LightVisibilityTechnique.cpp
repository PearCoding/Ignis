#include "LightVisibilityTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
LightVisibilityTechnique::LightVisibilityTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("lightvisibility")
    , mTechnique(obj)
{
    mLightSelector      = obj->property("light_selector").getString();
    mNoConnectionFactor = obj->property("no_connection_factor").getNumber(0.0f); // Set to zero to disable contribution of points not connected to a light
}

TechniqueInfo LightVisibilityTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights                = true;
    info.Variants[0].PrimaryPayloadCount       = 1;
    info.Variants[0].SecondaryPayloadCount     = 1;
    info.Variants[0].ShadowHandlingMode        = ShadowHandlingMode::AdvancedWithMaterials;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_lv_raypayload)";
    return info;
}

void LightVisibilityTechnique::generateBody(const SerializationInput& input) const
{
    input.Tree.addInteger("max_depth", *mTechnique, DefaultMaxRayDepth, ShadingTree::IntegerOptions::Dynamic().MakeGlobal());
    input.Tree.addNumber("no_connection_factor", *mTechnique, 0.0f, ShadingTree::NumberOptions::Zero().MakeGlobal());

    input.Stream << input.Tree.pullHeader()
                 << input.Tree.context().Lights->generateLightSelector(mLightSelector, input.Tree)
                 << "  let technique = make_lv_renderer(tech_max_depth, light_selector, tech_no_connection);" << std::endl;
}

} // namespace IG