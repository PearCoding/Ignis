#pragma once

#include "Target.h"

namespace IG {
enum Configuration : uint64 {
	IG_C_DEVICE_GENERIC		   = 0x00000001,
	IG_C_DEVICE_ASIMD		   = 0x00000002,
	IG_C_DEVICE_SSE42		   = 0x00000004,
	IG_C_DEVICE_AVX			   = 0x00000008,
	IG_C_DEVICE_AVX2		   = 0x00000010,
	IG_C_DEVICE_AVX512		   = 0x00000020,
	IG_C_DEVICE_NVVM_STREAMING = 0x00000100,
	IG_C_DEVICE_NVVM_MEGA	   = 0x00000200,
	IG_C_DEVICE_AMD_STREAMING  = 0x00000400,
	IG_C_DEVICE_AMD_MEGA	   = 0x00000800,
	IG_C_MASK_DEVICE		   = 0x00000FFF,

	IG_C_CAMERA_PERSPECTIVE = 0x00001000,
	IG_C_CAMERA_ORTHOGONAL	= 0x00002000,
	IG_C_CAMERA_FISHLENS	= 0x00004000,
	IG_C_CAMERA_LIST		= 0x00008000,
	IG_C_MASK_CAMERA		= 0x0000F000,

	IG_C_RENDERER_PATH	= 0x00010000,
	IG_C_RENDERER_DEBUG = 0x00020000,
	IG_C_RENDERER_AO    = 0x00040000,
	IG_C_MASK_RENDERER	= 0x000F0000,

	IG_C_NO_INSTANCES = 0x00100000,
};

constexpr uint64 DefaultConfiguration = IG_C_DEVICE_GENERIC | IG_C_CAMERA_PERSPECTIVE | IG_C_RENDERER_PATH;

inline Configuration targetToConfiguration(Target target)
{
	switch (target) {
	default:
	case Target::GENERIC:
		return IG_C_DEVICE_GENERIC;
	case Target::AVX512:
		return IG_C_DEVICE_AVX512;
	case Target::AVX2:
		return IG_C_DEVICE_AVX2;
	case Target::AVX:
		return IG_C_DEVICE_AVX;
	case Target::SSE42:
		return IG_C_DEVICE_SSE42;
	case Target::ASIMD:
		return IG_C_DEVICE_ASIMD;
	case Target::NVVM_STREAMING:
		return IG_C_DEVICE_NVVM_STREAMING;
	case Target::NVVM_MEGAKERNEL:
		return IG_C_DEVICE_NVVM_MEGA;
	case Target::AMDGPU_STREAMING:
		return IG_C_DEVICE_AMD_STREAMING;
	case Target::AMDGPU_MEGAKERNEL:
		return IG_C_DEVICE_AMD_MEGA;
	}
}

inline Target configurationToTarget(uint64 config)
{
	if (config & IG_C_DEVICE_GENERIC)
		return Target::GENERIC;
	else if (config & IG_C_DEVICE_ASIMD)
		return Target::ASIMD;
	else if (config & IG_C_DEVICE_SSE42)
		return Target::SSE42;
	else if (config & IG_C_DEVICE_AVX)
		return Target::AVX;
	else if (config & IG_C_DEVICE_AVX2)
		return Target::AVX2;
	else if (config & IG_C_DEVICE_AVX512)
		return Target::AVX512;
	else if (config & IG_C_DEVICE_NVVM_STREAMING)
		return Target::NVVM_STREAMING;
	else if (config & IG_C_DEVICE_NVVM_MEGA)
		return Target::NVVM_MEGAKERNEL;
	else if (config & IG_C_DEVICE_AMD_STREAMING)
		return Target::AMDGPU_STREAMING;
	else if (config & IG_C_DEVICE_AMD_MEGA)
		return Target::AMDGPU_MEGAKERNEL;
	else
		return Target::INVALID;
}

std::string configurationToString(uint64 config);
} // namespace IG