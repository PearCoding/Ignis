#include "LightVisibilityTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
LightVisibilityTechnique::LightVisibilityTechnique(const Parser::Object& obj)
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
    info.Variants[0].ShadowHandlingMode        = ShadowHandlingMode::Advanced;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_lv_raypayload)";
    return info;
}

void LightVisibilityTechnique::generateBody(const SerializationInput& input) const
{
    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree)
                 << "  let technique = make_lv_renderer(" << mMaxDepth << ", light_selector, " << mNoConnectionFactor << ");" << std::endl;
}

} // namespace IG