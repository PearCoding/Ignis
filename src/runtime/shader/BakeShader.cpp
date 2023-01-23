#include "BakeShader.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
std::string BakeShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_bake_shader(settings: &Settings, output: &mut [f32]) -> () {" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx) << std::endl;

    return stream.str();
}

std::string BakeShader::end()
{
    return "}";
}

std::string BakeShader::setupTexture2d(const LoaderContext& ctx, const std::string& expr, size_t width, size_t height)
{
    std::stringstream stream;
    stream << begin(ctx)
           << expr << std::endl
           << "  bake_texture2d(device, main_func, " << width << ", " << height << ", output);" << std::endl
           << end() << std::endl;
    return stream.str();
}

std::string BakeShader::setupConstantColor(const std::string& expr)
{
    std::stringstream stream;
    stream << "#[export] fn ig_constant_color(r:&mut f32,g:&mut f32,b:&mut f32,a:&mut f32) -> () {" << std::endl
           << expr << std::endl
           << "  let color = main_func();" << std::endl
           << "  *r=color.r; *g=color.g; *b=color.b; *a=color.a;" << std::endl
           << "}" << std::endl;
    return stream.str();
}

std::string BakeShader::setupConstantNumber(const std::string& expr)
{
    std::stringstream stream;
    stream << "#[export] fn ig_constant_number() -> f32 {" << std::endl
           << expr << std::endl
           << "  main_func()" << std::endl
           << "}" << std::endl;
    return stream.str();
}
} // namespace IG