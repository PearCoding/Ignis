#include "DebugTechnique.h"

namespace IG {
DebugTechnique::DebugTechnique()
    : Technique("debug")
{
}

TechniqueInfo DebugTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true; // We make use of the emissive information!
    return info;
}

void DebugTechnique::generateBody(const SerializationInput& input) const
{
    // The global parameter "__debug_mode" is set by the igview frontend to allow interactive controls

    // TODO: Maybe add a changeable default mode?
    input.Stream << "  let debug_mode = registry::get_global_parameter_i32(\"__debug_mode\", 0);" << std::endl
                 << "  let technique  = make_debug_renderer(debug_mode);" << std::endl;
}

} // namespace IG