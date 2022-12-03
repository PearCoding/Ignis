#include "UtilityShader.h"
#include "Logger.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string UtilityShader::setupTonemap(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_tonemap_shader(settings: &Settings, in_pixels: &[f32], out_pixels: &mut [u32], width: i32, height: i32, tonemap_settings: &TonemapSettings) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
           << "  ig_tonemap_pipeline(device, in_pixels, out_pixels, width, height, tonemap_settings)" << std::endl
           << "}";

    return stream.str();
}

std::string UtilityShader::setupImageinfo(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_imageinfo_shader(settings: &Settings, in_pixels: &[f32], width: i32, height: i32, ii_settings: &ImageInfoSettings, output: &mut ImageInfoOutput) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Options.Target) << std::endl
           << "  ig_imageinfo_pipeline(device, in_pixels, width, height, ii_settings, output)" << std::endl
           << "}";

    return stream.str();
}
} // namespace IG