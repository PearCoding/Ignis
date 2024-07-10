#pragma once

#include "IG_Config.h"

#include <chrono>
#include <mutex>
#include <streambuf>
#include <tuple>
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

    /// Close any additional console started due to the console subsystem (only relevant on Windows)
    void closeExtraConsole();
    /// Open a new console if none exists. Currently only supported on Windows
    void openConsole();

    std::ostream& startEntry(LogLevel level);

    static Logger& instance();

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
    int i           = 0;
    double mantissa = (double)mem.value();
    for (; i < 9 && mantissa >= 1024; ++i)
        mantissa /= 1024;

    mantissa = std::ceil(mantissa * 10) / 10.0;
    stream << mantissa << "BkMGTPEZY"[i];

    return i == 0 ? stream : (stream << "B");
}

/// Format vectors givin in row order, columnwise for better reading
template <typename Scalar, int Rows>
class FormatVector {
public:
    using Vector = Eigen::Matrix<Scalar, Rows, 1>;

    inline explicit FormatVector(const Vector& vec)
        : mValue(vec)
    {
    }

    inline const Vector& value() const { return mValue; }

private:
    Vector mValue;
};

template <typename Scalar, int Rows>
inline std::ostream& operator<<(std::ostream& stream, const FormatVector<Scalar, Rows>& vec)
{
    static Eigen::IOFormat format(Eigen::StreamPrecision, Eigen::DontAlignCols, ", ", "", "", "", "", "", ' ');
    stream << "[" << vec.value().transpose().format(format) << "]";
    return stream;
}

// Time printer based on https://stackoverflow.com/questions/22063979/elegant-time-print-in-c11

namespace detail {
template <typename>
struct duration_traits {
};

#define DURATION_TRAITS(Duration, Singular, Plural, Suffix) \
    template <>                                             \
    struct duration_traits<Duration> {                      \
        constexpr static const char* singular = Singular;   \
        constexpr static const char* plural   = Plural;     \
        constexpr static const char* suffix   = Suffix;     \
    }

DURATION_TRAITS(std::chrono::nanoseconds, "nanosecond", "nanoseconds", "ns");
DURATION_TRAITS(std::chrono::microseconds, "microsecond", "microseconds", "us");
DURATION_TRAITS(std::chrono::milliseconds, "millisecond", "milliseconds", "ms");
DURATION_TRAITS(std::chrono::seconds, "second", "seconds", "s");
DURATION_TRAITS(std::chrono::minutes, "minute", "minutes", "min");
DURATION_TRAITS(std::chrono::hours, "hour", "hours", "h");

#undef DURATION_TRAITS

using divisions = std::tuple<std::chrono::nanoseconds,
                             std::chrono::microseconds,
                             std::chrono::milliseconds,
                             std::chrono::seconds,
                             std::chrono::minutes,
                             std::chrono::hours>;

template <typename...>
struct print_duration_impl_ {
};

template <typename Head, typename... Tail>
struct print_duration_impl_<Head, Tail...> {
    template <typename Duration>
    static int print(std::ostream& os, Duration& dur)
    {
        const auto number_of_prints = print_duration_impl_<Tail...>::print(os, dur);

        const auto n     = std::chrono::duration_cast<Head>(dur);
        const auto count = n.count();

        if (count == 0) {
            // Only increase after the first print
            return number_of_prints > 0 ? number_of_prints + 1 : number_of_prints;
        }

        if (number_of_prints == 1) {
            os << ' ';
        } else if (number_of_prints > 1) {
            // Only print maximum of two units
            return number_of_prints;
        }

        using traits = duration_traits<Head>;
        os << count << traits::suffix;
        dur -= n;

        return number_of_prints + 1;
    }
};

template <>
struct print_duration_impl_<> {
    template <typename Duration>
    static int print(std::ostream& os, Duration& dur)
    {
        IG_UNUSED(os, dur);
        return 0;
    }
};

template <typename...>
struct print_duration {
};

template <typename... Args>
struct print_duration<std::tuple<Args...>> {
    template <typename Duration>
    static void print(std::ostream& os, Duration dur)
    {
        print_duration_impl_<Args...>::print(os, dur);
    }
};
} // namespace detail

template <typename Rep, typename Period>
std::ostream& operator<<(std::ostream& os, const std::chrono::duration<Rep, Period>& dur)
{
    detail::print_duration<detail::divisions>::print(os, dur);
    return os;
}

} // namespace IG

#define IG_LOGGER (IG::Logger::instance())
#define IG_LOG_UNSAFE(l) (IG::Logger::log((l)))
#define IG_LOG(l) (IG::Logger::threadsafe().log((l)))
