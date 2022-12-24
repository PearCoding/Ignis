#include "Logger.h"
#include "log/ConsoleLogListener.h"

#include <iostream>

namespace IG {
Logger::Logger()
    : mConsoleLogListener(std::make_shared<ConsoleLogListener>(true))
#ifdef IG_DEBUG
    , mVerbosity(L_DEBUG)
#else
    , mVerbosity(L_INFO)
#endif
    , mQuiet(false)
    , mEmptyStreamBuf(*this, true)
    , mEmptyStream(&mEmptyStreamBuf)
    , mStreamBuf(*this, false)
    , mStream(&mStreamBuf)
{
    addListener(mConsoleLogListener);
}

Logger& Logger::operator=(const Logger& other)
{
    if (this == &other)
        return *this;

    mVerbosity          = other.mVerbosity;
    mQuiet              = other.mQuiet;
    mConsoleLogListener = other.mConsoleLogListener;
    mListener           = other.mListener;
    return *this;
}

Logger& Logger::instance()
{
    static Logger this_log;
    return this_log;
}

static const char* const levelStr[] = {
    "Debug  ",
    "Info   ",
    "Warning",
    "Error  ",
    "Fatal  "
};

const char* Logger::levelString(LogLevel l)
{
    return levelStr[l];
}

void Logger::setQuiet(bool b)
{
    if (mQuiet == b)
        return;

    if (!b)
        addListener(mConsoleLogListener);
    else
        removeListener(mConsoleLogListener);
    mQuiet = b;
}

void Logger::enableAnsiTerminal(bool b)
{
    mConsoleLogListener->enableAnsi(b);
}

bool Logger::isUsingAnsiTerminal() const
{
    return mConsoleLogListener->isUsingAnsi();
}

void Logger::addListener(const std::shared_ptr<LogListener>& listener)
{
    mListener.push_back(listener);
}

void Logger::removeListener(const std::shared_ptr<LogListener>& listener)
{
    mListener.erase(std::remove(mListener.begin(), mListener.end(), listener), mListener.end());
}

std::ostream& Logger::startEntry(LogLevel level)
{
    if ((int)level < (int)verbosity())
        return mEmptyStream;

    for (const auto& listener : mListener)
        listener->startEntry(level);

    return mStream;
}

std::streambuf::int_type Logger::StreamBuf::overflow(std::streambuf::int_type c)
{
    if (mIgnore)
        return 0;

    for (const auto& listener : mLogger.mListener)
        listener->writeEntry((char)c);

    return 0;
}
} // namespace IG