#include "VolumePathTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
VolumePathTechnique::VolumePathTechnique(const Parser::Object& obj)
    : Technique("volpath")
{
    mMaxDepth      = obj.property("max_depth").getInteger(DefaultMaxRayDepth);
    mLightSelector = obj.property("light_selector").getString();
    mClamp         = obj.property("clamp").getNumber(0.0f);
}

TechniqueInfo VolumePathTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights                = true;
    info.Variants[0].UsesMedia                 = true;
    info.Variants[0].PrimaryPayloadCount       = 7;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_vpt_raypayload)";

    return info;
}

void VolumePathTechnique::generateBody(const SerializationInput& input) const
{
    ShadingTree tree(input.Context);
    input.Stream << input.Context.Lights->generateLightSelector(mLightSelector, tree)
                 << "  let aovs = @|_id:i32| make_empty_aov_image();" << std::endl
                 << "  let technique = make_volume_path_renderer(" << mMaxDepth << ", light_selector, media, aovs, " << mClamp << ");" << std::endl;
}

} // namespace IG