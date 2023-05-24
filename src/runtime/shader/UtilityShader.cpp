#include "UtilityShader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
std::string UtilityShader::setupTonemap(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_tonemap_shader(settings: &Settings, in_pixels: &[f32], out_pixels: &mut [u32], width: i32, height: i32, tonemap_settings: &TonemapSettings) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl
           << "  ig_tonemap_pipeline(device, in_pixels, out_pixels, width, height, tonemap_settings)" << std::endl
           << "}";

    return stream.str();
}

std::string UtilityShader::setupGlare(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_glare_shader(settings: &Settings, in_pixels: &[f32], out_pixels: &mut [u32], width: i32, height: i32, glare_settings: &GlareSettings, output: &mut GlareOutput) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl
           << "  ig_glare_pipeline(device, in_pixels, out_pixels, width, height, glare_settings, output)" << std::endl
           << "}";

    return stream.str();
}

std::string UtilityShader::setupImageinfo(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_imageinfo_shader(settings: &Settings, in_pixels: &[f32], width: i32, height: i32, ii_settings: &ImageInfoSettings, output: &mut ImageInfoOutput) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl
           << "  ig_imageinfo_pipeline(device, in_pixels, width, height, ii_settings, output)" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG