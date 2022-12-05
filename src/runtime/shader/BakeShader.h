#pragma once

#include "loader/LoaderContext.h"

namespace IG {
struct BakeShader {
    /// Will generate function specification and device
    static std::string begin(const LoaderContext& ctx);
    /// Will close out the shader
    static std::string end();

    /// Will construct a script baking a texture with the given size. Signature is `fn ig_bake_shader(&Settings, &mut [f32])`
    static std::string setupTexture2d(const LoaderContext& ctx, const std::string& expr, size_t width, size_t height);

    /// Special purpose shader with no device. Signature is `fn ig_constant_color(&mut f32, &mut f32, &mut f32, &mut f32)`
    using ConstantColorFunc = void(*)(float*, float*,float*,float*);
    static std::string setupConstantColor(const std::string& expr);

    /// Special purpose shader with no device. Signature is `fn ig_constant_number() -> f32`
    using ConstantNumberFunc = float(*)();
    static std::string setupConstantNumber(const std::string& expr);
};
} // namespace IG