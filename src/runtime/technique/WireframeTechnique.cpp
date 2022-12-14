#include "WireframeTechnique.h"

namespace IG {
WireframeTechnique::WireframeTechnique()
    : Technique("wireframe")
{
}

TechniqueInfo WireframeTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].RequiresExplicitCamera    = true; // We make use of the camera differential!
    info.Variants[0].PrimaryPayloadCount       = 2;
    info.Variants[0].EmitterPayloadInitializer = "make_simple_payload_initializer(init_wireframe_raypayload)";
    return info;
}

void WireframeTechnique::generateBody(const SerializationInput& input) const
{
    // `camera` was defined by RequiresExplicitCamera flag
    input.Stream << "  let technique = make_wireframe_renderer(camera);" << std::endl;
}

} // namespace IG