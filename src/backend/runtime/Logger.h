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
class Logger {
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
        inline LogClosure(Logger& logger)
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
            if((int)level >= (int)mLogger.verbosity()) {
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
    ~Logger();

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
        static Logger log;
        return log;
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
} // namespace IG

#define IG_LOGGER (IG::Logger::instance())
#define IG_LOG_UNSAFE(l) (IG::Logger::log((l)))
#define IG_LOG(l) (IG::Logger::threadsafe().log((l)))
