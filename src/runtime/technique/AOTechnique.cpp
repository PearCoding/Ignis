#include "AOTechnique.h"

namespace IG {
AOTechnique::AOTechnique()
    : Technique("ao")
{
}

TechniqueInfo AOTechnique::getInfo(const LoaderContext&) const
{
    return TechniqueInfo();
}

void AOTechnique::generateBody(const SerializationInput& input) const
{
    input.Stream << "  let technique = make_ao_renderer();" << std::endl;
}

} // namespace IG