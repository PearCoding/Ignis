#pragma once

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

inline bool require_padding(Target target)
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
} // namespace IG