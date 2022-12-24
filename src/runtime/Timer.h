#pragma once

#include "IG_Config.h"

#include <chrono>

namespace IG {
template <typename Clock>
class TimerBase {
public:
    using duration   = typename Clock::duration;
    using time_point = typename Clock::time_point;

    /**
     * @brief Construct a new timer without starting it
     */
    inline TimerBase()
        : mStart()
        , mEnd()
        , mRunning(false)
    {
    }

    /**
     * @brief Resets the timer. An already running timer will stop
     */
    inline void reset()
    {
        *this = TimerBase();
    }

    /**
     * @brief Will start the timer if it is not running already
     */
    inline void start()
    {
        if (mRunning)
            return;

        mStart   = Clock::now();
        mRunning = true;
    }

    /**
     * @brief Will stop the timer if it is running
     * 
     * @return The duration the timer was run between the most recent start and stop calls
     */
    inline duration stop()
    {
        if (mRunning) {
            mEnd     = Clock::now();
            mRunning = false;
        }

        return mEnd - mStart;
    }

    /**
     * @brief Will return the duration of the timer at for *now* if it is running, else the last stop call
     * 
     * @return The duration the timer was run between the most recent start and this call or the last stop call if it is not running
     */
    inline duration peek() const
    {
        return mRunning ? Clock::now() - mStart : mEnd - mStart;
    }

    /**
     * @brief Returns true if the timer is running
     */
    inline bool isRunning() const { return mRunning; }

    /**
     * @brief Same as stop(), but will return duration in milliseconds
     * 
     * @return Duration in milliseconds 
     */
    inline size_t stopMS()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(stop()).count();
    }

    /**
     * @brief Same as peek(), but will return duration in milliseconds
     * 
     * @return Duration in milliseconds 
     */
    inline size_t peekMS() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(peek()).count();
    }

private:
    time_point mStart;
    time_point mEnd;
    bool mRunning;
};

using Timer = TimerBase<std::chrono::high_resolution_clock>;
} // namespace IG