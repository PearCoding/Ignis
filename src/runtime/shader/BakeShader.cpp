#include "BakeShader.h"
#include "ShaderUtils.h"

#include <sstream>

namespace IG {
using namespace Parser;

std::string BakeShader::begin(const LoaderContext& ctx)
{
    std::stringstream stream;

    stream << "#[export] fn ig_bake_shader(settings: &Settings, output: &mut [f32]) -> () {" << std::endl
           << "  maybe_unused(settings);" << std::endl
           << "  " << ShaderUtils::constructDevice(ctx.Target) << std::endl;

    return stream.str();
}

std::string BakeShader::end()
{
    return "}";
}
} // namespace IG