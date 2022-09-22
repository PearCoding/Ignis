#pragma once

#include "IG_Config.h"

#include <mutex>
#include <streambuf>
#include <vector>

namespace IG {
enum LogLevel {
    L_DEBUG = 0,
    L_INFO,
    L_WARNING,
    L_ERROR,
    L_FATAL
};

class LogListener;
class ConsoleLogListener;
class IG_LIB Logger {
public:
    class StreamBuf final : public std::streambuf {
    public:
        inline StreamBuf(Logger& logger, bool ignore)
            : std::streambuf()
            , mLogger(logger)
            , mIgnore(ignore)
        {
        }
        std::streambuf::int_type overflow(std::streambuf::int_type c);

    private:
        Logger& mLogger;
        bool mIgnore;
    };

    // A closure used to make calls threadsafe. Never capture the ostream, else threadsafety is not guaranteed
    class LogClosure {
    public:
        inline explicit LogClosure(Logger& logger)
            : mLogger(logger)
            , mStarted(false)
        {
        }

        inline ~LogClosure()
        {
            if (mStarted)
                mLogger.mMutex.unlock();
        }

        inline std::ostream& log(LogLevel level)
        {
            if ((int)level >= (int)mLogger.verbosity()) {
                // Only lock if verbosity is high enough,
                // else nothing will be output anyway
                mStarted = true;
                mLogger.mMutex.lock();
            }
            return mLogger.startEntry(level);
        }

    private:
        Logger& mLogger;
        bool mStarted;
    };

    Logger();
    ~Logger() = default;

    Logger& operator=(const Logger&);

    static const char* levelString(LogLevel l);

    void addListener(const std::shared_ptr<LogListener>& listener);
    void removeListener(const std::shared_ptr<LogListener>& listener);

    inline void setVerbosity(LogLevel level) { mVerbosity = level; }
    inline LogLevel verbosity() const { return mVerbosity; }

    void setQuiet(bool b);
    inline bool isQuiet() const { return mQuiet; }

    void enableAnsiTerminal(bool b);
    bool isUsingAnsiTerminal() const;

    std::ostream& startEntry(LogLevel level);

    static inline Logger& instance()
    {
        static Logger this_log;
        return this_log;
    }

    static inline std::ostream& log(LogLevel level)
    {
        return instance().startEntry(level);
    }

    static inline LogClosure threadsafe()
    {
        return LogClosure(instance());
    }

private:
    std::vector<std::shared_ptr<LogListener>> mListener;
    std::shared_ptr<ConsoleLogListener> mConsoleLogListener;

    LogLevel mVerbosity;
    bool mQuiet;

    StreamBuf mEmptyStreamBuf;
    std::ostream mEmptyStream;

    StreamBuf mStreamBuf;
    std::ostream mStream;
    std::mutex mMutex;
};

template <typename T>
class FormatMemory {
public:
    inline explicit FormatMemory(const T& value)
        : mValue(value)
    {
    }

    inline const T& value() const { return mValue; }

private:
    T mValue;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& stream, const FormatMemory<T>& mem)
{
    int i = 0;
    double mantissa = (double)mem.value();
    for (; i < 9 && mantissa >= 1024; ++i)
        mantissa /= 1024;

    mantissa = std::ceil(mantissa * 10) / 10.0;
    stream << mantissa << "BkMGTPEZY"[i];

    return i == 0 ? stream : (stream << "B");
}
} // namespace IG

#define IG_LOGGER (IG::Logger::instance())
#define IG_LOG_UNSAFE(l) (IG::Logger::log((l)))
#define IG_LOG(l) (IG::Logger::threadsafe().log((l)))
