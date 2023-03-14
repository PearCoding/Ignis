#include "LightVisibilityTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
LightVisibilityTechnique::LightVisibilityTechnique(SceneObject& obj)
    : Technique("lightvisibility")
{
    mMaxDepth           = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mLightSelector      = obj.property("light_selector").getString();
    mNoConnectionFactor = obj.property("no_connection_factor").getNumber(0.0f); // Set to zero to disable contribution of points not connected to a light
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
    // Insert config into global registry
    input.Context.GlobalRegistry.IntParameters["__tech_max_depth"]       = (int)mMaxDepth;
    input.Context.GlobalRegistry.FloatParameters["__tech_no_connection"] = mNoConnectionFactor;

    if (mMaxDepth < 2) // 0 & 1 can be an optimization
        input.Stream << "  let tech_max_depth = " << mMaxDepth << ":i32;" << std::endl;
    else
        input.Stream << "  let tech_max_depth = registry::get_global_parameter_i32(\"__tech_max_depth\", 8);" << std::endl;

    if (mNoConnectionFactor <= 0) // 0 is a special case
        input.Stream << "  let tech_no_connection = " << mNoConnectionFactor << ":f32;" << std::endl;
    else
        input.Stream << "  let tech_no_connection = registry::get_global_parameter_f32(\"__tech_no_connection\", 0);" << std::endl;

    // Handle AOVs

    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree)
                 << "  let technique = make_lv_renderer(tech_max_depth, light_selector, tech_no_connection);" << std::endl;
}

} // namespace IG