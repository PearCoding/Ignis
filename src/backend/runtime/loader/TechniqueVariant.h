#pragma once

#include "IG_Config.h"

namespace IG {

using TechniqueVariantSelector = uint32 (*)(uint32);

template <typename T>
struct TechniqueVariantBase {
    T RayGenerationShader;
    T MissShader;
    std::vector<T> HitShaders;
    T AdvancedShadowHitShader;
    T AdvancedShadowMissShader;
};

using TechniqueVariant          = TechniqueVariantBase<std::string>;
using TechniqueVariantShaderSet = TechniqueVariantBase<void*>;
} // namespace IG