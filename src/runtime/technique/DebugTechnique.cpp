#include "DebugTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"

namespace IG {
DebugTechnique::DebugTechnique(const std::shared_ptr<SceneObject>& obj)
    : Technique("debug")
{
    IG_UNUSED(obj);
}

TechniqueInfo DebugTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.UsesLights = true; // We make use of the emissive information!
    return info;
}

void DebugTechnique::generateBody(const SerializationInput& input) const
{
    input.Stream << "  let technique  = make_debug_renderer(device, spi);" << std::endl;
}

} // namespace IG