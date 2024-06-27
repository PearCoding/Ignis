#include "ShaderGenerator.h"
#include "shader/ShaderUtils.h"

namespace IG {
std::string ShaderGenerator::generatePerspective(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_perspective_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateTonemap(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_tonemap_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG