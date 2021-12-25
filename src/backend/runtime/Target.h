#pragma once

#include "HW_Features.h"
#include "IG_Config.h"

namespace IG {
enum class Target : uint32 {
    GENERIC = 0,
    AVX512,
    AVX2,
    AVX,
    SSE42,
    ASIMD,
    NVVM,
    AMDGPU,
    INVALID
};

inline bool doesTargetRequirePadding(Target target)
{
    switch (target) {
    default:
        return false;
    case Target::NVVM:
    case Target::AMDGPU:
        return true;
    }
}

inline bool isCPU(Target target)
{
    switch (target) {
    default:
        return true;
    case Target::NVVM:
    case Target::AMDGPU:
        return false;
    }
}

inline Target getRecommendedCPUTarget()
{
#if defined(IG_HAS_HW_FEATURE_AVX512)
    return Target::AVX512;
#elif defined(IG_HAS_HW_FEATURE_AVX2)
    return Target::AVX2;
#elif defined(IG_HAS_HW_FEATURE_AVX)
    return Target::AVX;
#elif defined(IG_HAS_HW_FEATURE_SSE4_2)
    return Target::SSE42;
#elif defined(IG_HAS_HW_FEATURE_SSE2)
    return Target::ASIMD;
#else
    return Target::GENERIC;
#endif
}

inline const char* targetToString(Target target)
{
    switch (target) {
    case Target::GENERIC:
        return "Generic";
    case Target::AVX512:
        return "AVX512";
    case Target::AVX2:
        return "AVX2";
    case Target::AVX:
        return "AVX";
    case Target::SSE42:
        return "SSE4.2";
    case Target::ASIMD:
        return "ASIMD";
    case Target::NVVM:
        return "NVVM";
    case Target::AMDGPU:
        return "AMDGPU";
    case Target::INVALID:
        return "Invalid";
    default:
        return "Unknown";
    }
}
} // namespace IG