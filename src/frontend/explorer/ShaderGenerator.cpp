#include "ShaderGenerator.h"
#include "shader/ShaderUtils.h"

namespace IG {
std::string ShaderGenerator::generatePerspective(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_perspective_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateImageInfo(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_imageinfo_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateTonemap(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_tonemap_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateGlare(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_glare_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateOverlay(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_overlay_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}

std::string ShaderGenerator::generateAOV(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_aov_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG