#pragma once

#include "Statistics.h"
#include "device/DeviceTimer.h"

namespace IG {
class IG_LIB StatisticHandler {
public:
    explicit StatisticHandler(const anydsl::Device& device);

    void startIteration();
    void startFrame();

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

    [[nodiscard]] inline const Statistics& statistics() const { return mStatistics; }

    inline void enableStreamRecording(bool b = true) { mRecordingStream = b; }
    [[nodiscard]] inline bool isStreamRecordingEnabled() const { return mRecordingStream; }

private:
    void pushToStream(const SmallShaderKey& key);
    void pushToStream(const SectionType& key);

    anydsl::Device mDevice;

    bool mStarted;
    anydsl::Event mStartEvent;

    anydsl::Event mFrameEvent;
    bool mFrameSubmitted;
    anydsl::Event mIterationEvent;
    bool mIterationSubmitted;

    std::unordered_map<SmallShaderKey, DeviceTimer, SmallShaderKeyHash> mShaders;
    std::unordered_map<SmallShaderKey, size_t, SmallShaderKeyHash> mShaderPayload;
    std::vector<DeviceTimer> mSections;

    Statistics mStatistics;

    bool mRecordingStream;
};
} // namespace IG