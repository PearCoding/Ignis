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
	NVVM_STREAMING,
	NVVM_MEGAKERNEL,
	AMDGPU_STREAMING,
	AMDGPU_MEGAKERNEL,
	INVALID
};

inline bool doesTargetRequirePadding(Target target)
{
	switch (target) {
	default:
		return false;
	case Target::NVVM_STREAMING:
	case Target::NVVM_MEGAKERNEL:
	case Target::AMDGPU_STREAMING:
	case Target::AMDGPU_MEGAKERNEL:
		return true;
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
} // namespace IG