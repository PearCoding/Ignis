#include "UtilityShader.h"
#include "Logger.h"
#include "ShaderUtils.h"
#include "loader/LoaderCamera.h"

#include <sstream>

namespace IG {
std::string UtilityShader::setupTonemap(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_tonemap_shader(settings: &Settings, tonemap_settings: &TonemapSettings, out_pixels: &mut [u32]) -> () {" << std::endl
           << ShaderUtils::constructDevice(ctx.Options) << std::endl
           << "  ig_tonemap_pipeline_std(device, *tonemap_settings, out_pixels)" << std::endl
           << "}";

    return stream.str();
}

std::string UtilityShader::setupImageinfo(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_imageinfo_shader(settings: &Settings, ii_settings: &ImageInfoSettings, output: &mut ImageInfoOutput) -> () {" << std::endl
           << ShaderUtils::constructDevice(ctx.Options) << std::endl
           << "  ig_imageinfo_pipeline_std(device, *ii_settings, output)" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG