#include "InfoBufferTechnique.h"
#include "loader/LoaderContext.h"
#include "loader/Parser.h"

namespace IG {
void InfoBufferTechnique::enable(TechniqueInfo& info)
{
    IG_UNUSED(info);
}

bool InfoBufferTechnique::insertBody(const Technique::SerializationInput& input)
{
    input.Stream << "  let full_technique = wrap_infobuffer_renderer(device, settings.iter, spi, technique);" << std::endl;
    return true;
}

} // namespace IG