#pragma once

#include "StatisticHandler.h"

namespace IG {
class IG_LIB TieStatisticHandler {
public:
    TieStatisticHandler()
        : mFirstHandler()
        , mSecondHandler()
    {
    }

    inline void setDevices(const anydsl::Device& firstDevice, const anydsl::Device& secondDevice)
    {
        mFirstHandler.setDevice(firstDevice);
        mSecondHandler.setDevice(secondDevice);
    }

    inline void reset()
    {
        mFirstHandler.reset();
        mSecondHandler.reset();
    }

    inline void startIteration()
    {
        mFirstHandler.startIteration();
        mSecondHandler.startIteration();
    }

    inline void beginShaderLaunch(ShaderType type, size_t workload, size_t id)
    {
        mFirstHandler.beginShaderLaunch(type, workload, id);
        mSecondHandler.beginShaderLaunch(type, workload, id);
    }

    inline void endShaderLaunch(ShaderType type, size_t id)
    {
        mFirstHandler.endShaderLaunch(type, id);
        mSecondHandler.endShaderLaunch(type, id);
    }

    inline void beginSection(SectionType type)
    {
        mFirstHandler.beginSection(type);
        mSecondHandler.beginSection(type);
    }

    inline void endSection(SectionType type)
    {
        mFirstHandler.endSection(type);
        mSecondHandler.endSection(type);
    }

    inline void increase(Quantity quantity, uint64 value)
    {
        mFirstHandler.increase(quantity, value);
        mSecondHandler.increase(quantity, value);
    }

    inline void finalize()
    {
        mFirstHandler.finalize();
        mSecondHandler.finalize();
    }

    inline const StatisticHandler& first() const { return mFirstHandler; }
    inline const StatisticHandler& second() const { return mSecondHandler; }

private:
    StatisticHandler mFirstHandler;
    StatisticHandler mSecondHandler;
};
} // namespace IG