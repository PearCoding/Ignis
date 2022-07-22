#pragma once

#include "IG_Config.h"

namespace IG {
enum class Target : uint32 {
    GENERIC = 0,
    SINGLE, // Debug target on CPU with single thread usage
    AVX512,
    AVX2,
    AVX,
    SSE42,
    ASIMD,
    NVVM,
    AMDGPU,
    INVALID
};

class TargetInfo {
public:
    const Target Value;

    inline explicit TargetInfo(Target target)
        : Value(target)
    {
    }

    [[nodiscard]] inline bool requiresPadding() const
    {
        switch (Value) {
        default:
            return false;
        case Target::NVVM:
        case Target::AMDGPU:
            return true;
        }
    }

    [[nodiscard]] inline bool isCPU() const
    {
        switch (Value) {
        default:
            return true;
        case Target::NVVM:
        case Target::AMDGPU:
            return false;
        }
    }

    [[nodiscard]] inline const char* toString() const
    {
        switch (Value) {
        case Target::GENERIC:
            return "Generic";
        case Target::SINGLE:
            return "SingleThreaded";
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
};

} // namespace IG