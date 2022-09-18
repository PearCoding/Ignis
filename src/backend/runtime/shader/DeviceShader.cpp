#include "DeviceShader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string DeviceShader::setup(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_callback_shader(settings: &Settings) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl
           << "  let spi = " << ShaderUtils::inlineSPI(ctx) << ";" << std::endl
           << "  let payload_info = PayloadInfo { primary_count = 0, secondary_count = 0 };" << std::endl // TODO
           << "  ig_render_pipeline(device, payload_info, spi)" << std::endl
           << "}";
    
    return stream.str();
}
} // namespace IG