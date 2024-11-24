#include "EnvCheckTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/LoaderLight.h"
#include "loader/ShadingTree.h"

namespace IG {
EnvCheckTechnique::EnvCheckTechnique()
    : Technique("envcheck")
{
}

TechniqueInfo EnvCheckTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true;
    
    info.EnabledAOVs.emplace_back("Pdf");
    return info;
}

void EnvCheckTechnique::generateBody(const SerializationInput& input) const
{
    input.Stream << input.Tree.context().Lights->generateLightSelector("uniform", input.Tree)
                 << "  let technique = make_env_check_renderer(device, spi, light_selector);" << std::endl;
}

} // namespace IG