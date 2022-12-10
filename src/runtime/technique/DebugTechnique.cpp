#include "DebugTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/Parser.h"

namespace IG {
DebugTechnique::DebugTechnique(const Parser::Object& obj)
    : Technique("debug")
    , mInitialDebugMode(DebugMode::Normal)
{
    const auto debug = stringToDebugMode(obj.property("mode").getString(""));
    if (debug.has_value())
        mInitialDebugMode = *debug;
}

TechniqueInfo DebugTechnique::getInfo(const LoaderContext&) const
{
    TechniqueInfo info;
    info.Variants[0].UsesLights = true; // We make use of the emissive information!
    return info;
}

void DebugTechnique::generateBody(const SerializationInput& input) const
{
    // The global parameter "__debug_mode" is modified by the igview frontend to allow interactive controls
    input.Context.GlobalRegistry.IntParameters["__debug_mode"] = (int)mInitialDebugMode;

    // TODO: Maybe add a changeable default mode?
    input.Stream << "  let debug_mode = registry::get_global_parameter_i32(\"__debug_mode\", 0);" << std::endl
                 << "  let technique  = make_debug_renderer(debug_mode);" << std::endl;
}

} // namespace IG