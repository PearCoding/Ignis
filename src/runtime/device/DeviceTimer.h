#pragma once

#include "AnyDSLRuntime.h"

namespace IG {

/// Basic state machine with two timers
class DeviceTimer {
    IG_CLASS_NON_COPYABLE(DeviceTimer);

public:
    enum class State {
        Idle,    // The actual process is not running
        Started, // We recorded the start
        Ended    // We recorded the end, but it may still run
    };

    /**
     * @brief Construct a new timer without starting it
     */
    inline DeviceTimer(const anydsl::Device& device)
        : mDevice(device)
        , mStart(device)
        , mEnd(device)
        , mState(State::Idle)
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
    }

    inline const anydsl::Event& start() const
    {
        return mStart;
    }

    inline const anydsl::Event& end() const
    {
        return mEnd;
    }

    inline State state() const { return mState; }

    /// @brief Will try to acquire elapsed milliseconds between start and end
    inline void finalize()
    {
        if (mState != State::Ended)
            return;
        mState = State::Idle;
    }

private:
    anydsl::Device mDevice;
    anydsl::Event mStart;
    anydsl::Event mEnd;

    State mState;
};
} // namespace IG