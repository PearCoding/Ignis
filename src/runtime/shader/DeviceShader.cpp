#include "DeviceShader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
std::string DeviceShader::setup(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_callback_shader(settings: &Settings) -> () {" << std::endl
           << ShaderUtils::constructDevice(ctx.Options) << std::endl
           << "  " << ShaderUtils::generateDatabase(ctx) << std::endl
           << "  " << ShaderUtils::inlineScene(ctx, ctx.Options.Specialization != RuntimeOptions::SpecializationMode::Disable) << std::endl
           << "  let payload_info = " << ShaderUtils::inlinePayloadInfo(ctx) << ";" << std::endl // TODO
           << "  ig_render_pipeline(device, scene, payload_info)" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG