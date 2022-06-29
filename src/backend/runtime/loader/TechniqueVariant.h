#pragma once

#include "RuntimeStructs.h"

namespace IG {
enum class CallbackType {
    BeforeIteration = 0,
    AfterIteration,
    _COUNT
};

template <typename T>
struct ShaderOutput {
    T Exec;
    ParameterSet LocalRegistry;
};

template <typename T>
struct TechniqueVariantBase {
    ShaderOutput<T> RayGenerationShader;
    ShaderOutput<T> MissShader;
    std::vector<ShaderOutput<T>> HitShaders;
    std::vector<ShaderOutput<T>> AdvancedShadowHitShaders;
    std::vector<ShaderOutput<T>> AdvancedShadowMissShaders;
    std::array<ShaderOutput<T>, (size_t)CallbackType::_COUNT> CallbackShaders{};
};

using TechniqueVariant          = TechniqueVariantBase<std::string>;
using TechniqueVariantShaderSet = TechniqueVariantBase<void*>;
} // namespace IG