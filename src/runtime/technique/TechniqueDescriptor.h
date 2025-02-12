#pragma once

#include "ParameterSet.h"

namespace IG {
enum class CallbackType {
    BeforePass = 0,
    AfterPass,
    _COUNT
};

template <typename T>
struct ShaderOutput {
    T Exec;
    std::shared_ptr<ParameterSet> LocalRegistry;
};

template <typename T>
struct TechniqueDescriptorBase {
    ShaderOutput<T> DeviceShader;
    ShaderOutput<T> TonemapShader;
    ShaderOutput<T> ImageinfoShader;
    ShaderOutput<T> PrimaryTraversalShader;
    ShaderOutput<T> SecondaryTraversalShader;
    ShaderOutput<T> RayGenerationShader;
    ShaderOutput<T> MissShader;
    std::vector<ShaderOutput<T>> HitShaders;
    std::vector<ShaderOutput<T>> AdvancedShadowHitShaders;
    std::vector<ShaderOutput<T>> AdvancedShadowMissShaders;
    std::array<ShaderOutput<T>, (size_t)CallbackType::_COUNT> CallbackShaders{};
};

using TechniqueDescriptorSourceSet = TechniqueDescriptorBase<std::string>;
using TechniqueDescriptorShaderSet = TechniqueDescriptorBase<void*>;
} // namespace IG