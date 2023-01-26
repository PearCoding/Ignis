#include "DeviceShader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
std::string DeviceShader::setup(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_callback_shader(settings: &Settings) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl // TODO
           << "  ig_render_pipeline(device, payload_info)" << std::endl
           << "}";
    
    return stream.str();
}
} // namespace IG