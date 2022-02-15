#pragma once

#include <map>
#include <string>

#include "Timer.h"

namespace IG {
enum class ShaderType {
    Device,
    RayGeneration,
    Hit,
    Miss,
    AdvancedShadowHit,
    AdvancedShadowMiss,
    Tonemap
};

class Statistics {
public:
    inline void reset()
    {
        *this = Statistics();
    }

    void beginShaderLaunch(ShaderType type, size_t id);
    void endShaderLaunch(ShaderType type, size_t id);

    void add(const Statistics& other);

    std::string dump(size_t iter, bool verbose) const;

private:
    struct ShaderStats {
        Timer timer;
        size_t elapsedMS = 0;
        size_t count     = 0;
    };

    ShaderStats* getStats(ShaderType type, size_t id);

    ShaderStats mDeviceStats;
    ShaderStats mRayGenerationStats;
    ShaderStats mMissStats;
    std::map<size_t, ShaderStats> mHitStats;
    ShaderStats mAdvancedShadowHitStats;
    ShaderStats mAdvancedShadowMissStats;
    ShaderStats mTonemapStats;
};
} // namespace IG