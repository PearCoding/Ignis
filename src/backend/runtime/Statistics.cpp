#include "Statistics.h"
#include <sstream>

namespace IG {
Statistics::Statistics()
    : mQuantities()
{
    std::fill(mQuantities.begin(), mQuantities.end(), 0);
}

void Statistics::beginShaderLaunch(ShaderType type, size_t id)
{
    ShaderStats* stats = getStats(type, id);
    stats->timer.start();
    stats->count++;
}

void Statistics::endShaderLaunch(ShaderType type, size_t id)
{
    ShaderStats* stats = getStats(type, id);
    stats->elapsedMS += stats->timer.stopMS();
}

void Statistics::add(const Statistics& other)
{
    const auto addStats = [](ShaderStats& a, const ShaderStats& b) {
        a.elapsedMS += b.elapsedMS;
        a.count += b.count;
    };

    addStats(mDeviceStats, other.mDeviceStats);
    addStats(mRayGenerationStats, other.mRayGenerationStats);
    addStats(mMissStats, other.mMissStats);
    for (const auto& pair : other.mHitStats)
        addStats(mHitStats[pair.first], pair.second);
    addStats(mAdvancedShadowHitStats, other.mAdvancedShadowHitStats);
    addStats(mAdvancedShadowMissStats, other.mAdvancedShadowMissStats);
    addStats(mTonemapStats, other.mTonemapStats);
    addStats(mImageInfoStats, other.mImageInfoStats);

    for (size_t i = 0; i < other.mQuantities.size(); ++i)
        mQuantities[i] += other.mQuantities[i];
}

std::string Statistics::dump(size_t totalMS, size_t iter, bool verbose) const
{
    const auto dumpInline = [=](size_t count, size_t elapsedMS) {
        std::stringstream bstream;
        bstream << elapsedMS << "ms [" << count << "]";
        if (iter != 0)
            bstream << " | " << elapsedMS / iter << "ms [" << count / iter << "] per Iteration";
        return bstream.str();
    };

    const auto dumpStats = [=](const ShaderStats& stats) {
        return dumpInline(stats.count, stats.elapsedMS);
    };

    const auto dumpQuantity = [=](size_t count) {
        std::stringstream bstream;
        bstream << count / totalMS << " per ms [" << count << "]";
        return bstream.str();
    };

    // Get all hits information
    size_t hitsCount = 0;
    size_t hitsMS    = 0;
    for (const auto& pair : mHitStats) {
        hitsCount += pair.second.count;
        hitsMS += pair.second.elapsedMS;
    }

    // Dump
    std::stringstream stream;
    stream << "Statistics:" << std::endl
           << "  Shader:" << std::endl
           << "    Device>        " << dumpStats(mDeviceStats) << std::endl
           << "    RayGeneration> " << dumpStats(mRayGenerationStats) << std::endl
           << "    Miss>          " << dumpStats(mMissStats) << std::endl
           << "    Hits>          " << dumpInline(hitsCount, hitsMS) << std::endl;

    if (verbose) {
        for (const auto& pair : mHitStats)
            stream << "      @" << pair.first << " " << dumpStats(pair.second) << std::endl;
    }

    if (mAdvancedShadowHitStats.count > 0 || mAdvancedShadowMissStats.count > 0) {
        stream << "    AdvancedShadow>" << std::endl
               << "      Miss> " << dumpStats(mAdvancedShadowMissStats) << std::endl
               << "      Hits> " << dumpStats(mAdvancedShadowHitStats) << std::endl;
    }

    if (mImageInfoStats.count > 0)
        stream << "    ImageInfo>     " << dumpStats(mImageInfoStats) << std::endl;

    if (mTonemapStats.count > 0)
        stream << "    Tonemap>       " << dumpStats(mTonemapStats) << std::endl;

    stream << "  Quantities:" << std::endl
           << "    CameraRays>  " << dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount]) << std::endl
           << "    ShadowRays>  " << dumpQuantity(mQuantities[(size_t)Quantity::ShadowRayCount]) << std::endl
           << "    BounceRays>  " << dumpQuantity(mQuantities[(size_t)Quantity::BounceRayCount]) << std::endl
           << "    PrimaryRays> " << dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount] + mQuantities[(size_t)Quantity::BounceRayCount]) << std::endl
           << "    TotalRays>   " << dumpQuantity(mQuantities[(size_t)Quantity::CameraRayCount] + mQuantities[(size_t)Quantity::BounceRayCount] + mQuantities[(size_t)Quantity::ShadowRayCount]) << std::endl;

    return stream.str();
}

Statistics::ShaderStats* Statistics::getStats(ShaderType type, size_t id)
{
    switch (type) {
    default:
    case ShaderType::Device:
        return &mDeviceStats;
    case ShaderType::RayGeneration:
        return &mRayGenerationStats;
    case ShaderType::Miss:
        return &mMissStats;
    case ShaderType::Hit:
        return &mHitStats[id];
    case ShaderType::AdvancedShadowHit:
        return &mAdvancedShadowHitStats;
    case ShaderType::AdvancedShadowMiss:
        return &mAdvancedShadowMissStats;
    case ShaderType::Tonemap:
        return &mTonemapStats;
    case ShaderType::ImageInfo:
        return &mImageInfoStats;
    }
}
} // namespace IG