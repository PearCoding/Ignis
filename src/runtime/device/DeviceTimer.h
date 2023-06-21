#pragma once

#include "AnyDSLRuntime.h"

namespace IG {
class DeviceTimer {
    IG_CLASS_NON_COPYABLE(DeviceTimer);

public:
    /**
     * @brief Construct a new timer without starting it
     */
    inline DeviceTimer(const anydsl::Device& device)
        : mDevice(device)
        , mStart(device)
        , mEnd(device)
        , mState(State::Idle)
        , mElapsedMS(0)
    {
    }

    DeviceTimer(DeviceTimer&& timer)            = default;
    DeviceTimer& operator=(DeviceTimer&& timer) = default;

    /**
     * @brief Resets the timer. An already running timer will stop
     */
    inline void reset()
    {
        *this = std::move(DeviceTimer(mDevice));
    }

    /**
     * @brief Will start the timer if it is not running already
     */
    inline void recordStart()
    {
        if (mState == State::Ended)
            tryFinalize();

        IG_ASSERT(mState == State::Idle, "Expected device timer to be in a correct state");

        mStart.record();
        mState = State::Started;
    }

    /**
     * @brief Will record a stop of the timer. This will not necessarily stop the execution.
     */
    inline void recordStop()
    {
        IG_ASSERT(mState == State::Started, "Expected device timer to have started already");

        mEnd.record();
        mState = State::Ended;
        tryFinalize();
    }

    inline float elapsedMS() const
    {
        return mElapsedMS;
    }

    /// @brief Will try to acquire elapsed milliseconds between start and end
    inline void tryFinalize()
    {
        if (mState != State::Ended)
            return;

        float elapsed = anydsl::Event::elapsedTimeMS(mStart, mEnd);
        if (elapsed < 0)
            return;

        mElapsedMS += elapsed;
        mState = State::Idle;
    }

private:
    anydsl::Device mDevice;
    anydsl::Event mStart;
    anydsl::Event mEnd;

    enum class State {
        Idle,    // The actual process is not running
        Started, // We recorded the start
        Ended    // We recorded the end, but it may still run
    };
    State mState;

    float mElapsedMS;
};
} // namespace IG