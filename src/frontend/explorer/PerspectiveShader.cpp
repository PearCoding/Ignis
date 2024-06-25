#include "PerspectiveShader.h"
#include "shader/ShaderUtils.h"

namespace IG {
std::string PerspectiveShader::generate(const LoaderOptions& options)
{
    std::stringstream stream;

    stream << "#[export] fn ig_pass_main(settings: &Settings) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(options) << std::endl
           << "  handle_perspective_shader(device, spi, settings);" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG