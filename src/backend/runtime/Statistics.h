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
    Callback,
    Tonemap,
    ImageInfo
};

enum class Quantity {
    // This should be in sync with core/stats.art
    CameraRayCount = 0,
    ShadowRayCount,
    BounceRayCount,

    _COUNT
};

class Statistics {
public:
    Statistics();

    inline void reset()
    {
        *this = Statistics();
    }

    void beginShaderLaunch(ShaderType type, size_t workload, size_t id);
    void endShaderLaunch(ShaderType type, size_t id);

    inline void increase(Quantity quantity, uint64 value)
    {
        mQuantities[(size_t)quantity] += value;
    }

    void add(const Statistics& other);

    [[nodiscard]] std::string dump(size_t totalMS, size_t iter, bool verbose) const;

private:
    struct ShaderStats {
        Timer timer;
        size_t elapsedMS    = 0;
        size_t count        = 0;
        size_t workload     = 0; // This might overflow, but who cares for statistical stuff after that huge number of iterations
        size_t max_workload = 0;
        size_t min_workload = std::numeric_limits<size_t>::max();

        ShaderStats& operator+=(const ShaderStats& other);
    };

    [[nodiscard]] ShaderStats* getStats(ShaderType type, size_t id);

    ShaderStats mDeviceStats;
    ShaderStats mRayGenerationStats;
    ShaderStats mMissStats;
    std::map<size_t, ShaderStats> mHitStats;
    std::map<size_t, ShaderStats> mAdvancedShadowHitStats;
    std::map<size_t, ShaderStats> mAdvancedShadowMissStats;
    std::map<size_t, ShaderStats> mCallbackStats;
    ShaderStats mImageInfoStats;
    ShaderStats mTonemapStats;

    std::array<uint64, (size_t)Quantity::_COUNT> mQuantities;
};
} // namespace IG