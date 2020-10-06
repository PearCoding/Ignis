#pragma once

#include "IG_Config.h"

namespace IG {
enum class Target : uint32 {
	GENERIC = 0,
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
}