#pragma once

#include "Statistics.h"
#include "device/DeviceTimer.h"

namespace IG {
class IG_LIB StatisticHandler {
public:
    explicit StatisticHandler(const anydsl::Device& device = anydsl::Device());

    inline void setDevice(const anydsl::Device& device)
    {
        mDevice              = device; // Be careful with this.
        mStartIterationEvent = anydsl::Event(device);
        reset();
    }

    void startIteration();

    inline void reset()
    {
        *this = std::move(StatisticHandler(mDevice));
    }

    void beginShaderLaunch(ShaderType type, size_t workload, size_t id);
    void endShaderLaunch(ShaderType type, size_t id);

    void beginSection(SectionType type);
    void endSection(SectionType type);

    inline void increase(Quantity quantity, uint64 value)
    {
        mStatistics.add(quantity, value);
    }

    void finalize();
    inline const Statistics& statistics() const { return mStatistics; }

private:
    void pushToStream(const SmallShaderKey& key);
    void pushToStream(const SectionType& key);

    anydsl::Device mDevice;
    anydsl::Event mStartIterationEvent;

    std::unordered_map<SmallShaderKey, DeviceTimer, SmallShaderKeyHash> mShaders;
    std::unordered_map<SmallShaderKey, size_t, SmallShaderKeyHash> mShaderPayload;
    std::vector<DeviceTimer> mSections;

    Statistics mStatistics;
};
} // namespace IG